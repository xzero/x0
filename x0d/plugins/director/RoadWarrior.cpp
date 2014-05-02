#include "RoadWarrior.h"
#include "Backend.h"
#include "RequestNotes.h"
#include "HttpBackend.h"
#include "FastCgiBackend.h"
#include <x0/http/HttpRequest.h>

RoadWarrior::RoadWarrior(x0::HttpWorker* worker) :
    BackendManager(worker, "__roadwarrior__"),
    backendsLock_(),
    backends_()
{
}

RoadWarrior::~RoadWarrior()
{
    for (auto backend: backends_) {
        delete backend.second;
    }
}

Backend* RoadWarrior::acquireBackend(const x0::SocketSpec& spec, Type type)
{
    std::lock_guard<std::mutex> _l(backendsLock_);

    auto bi = backends_.find(spec);
    if (bi != backends_.end()) {
        return bi->second;
    }

    Backend* backend;
    switch (type) {
        case HTTP:
            backend = new HttpBackend(this, spec.str(), spec, 0, false);
            break;
        case FCGI:
            backend = new FastCgiBackend(this, spec.str(), spec, 0, false);
            break;
        default:
            return nullptr;
    }

    backends_[spec] = backend;
    return backend;
}

void RoadWarrior::handleRequest(RequestNotes* rn, const x0::SocketSpec& spec, Type type)
{
    Backend* backend = acquireBackend(spec, type);
    if (!backend) {
        rn->request->status = x0::HttpStatus::InternalServerError;
        rn->request->finish();
    }

    SchedulerStatus result = backend->tryProcess(rn);
    if (result != SchedulerStatus::Success) {
        rn->request->status = x0::HttpStatus::ServiceUnavailable;
        rn->request->finish();
    }
}

void RoadWarrior::reject(RequestNotes* rn)
{
    // this request couldn't be served by the backend, so finish it with a 503 (Service Unavailable).

    auto r = rn->request;
    r->status = x0::HttpStatus::ServiceUnavailable;
    r->finish();
}

void RoadWarrior::release(RequestNotes* rn)
{
    // The passed backend just finished serving a request, so we might now pass it a queued request,
    // in case we would support queuing (do we want that?).
}

void RoadWarrior::writeJSON(x0::JsonWriter& json) const
{
    json.beginObject(name());
    json.beginArray("members");
    for (auto backend: backends_) {
        json.value(*backend.second);
    }
    json.endArray();
    json.endObject();
}
