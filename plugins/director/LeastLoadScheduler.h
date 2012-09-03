/* <plugins/director/LeastLoadScheduler.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#pragma once

#include "Scheduler.h"
#include "Backend.h"
#include "RequestNotes.h"
#include <x0/TimeSpan.h>
#include <deque>
#include <ev++.h>

class RequestNotes;

class LeastLoadScheduler :
	public Scheduler
{
private:
	std::deque<x0::HttpRequest*> queue_; //! list of queued requests.
	ev::timer queueTimer_;

public:
	explicit LeastLoadScheduler(Director* d);
	~LeastLoadScheduler();

	virtual void schedule(x0::HttpRequest* r);
	virtual void reschedule(x0::HttpRequest* r);
	virtual void dequeueTo(Backend* backend);

	virtual bool load(x0::IniFile& settings);
	virtual bool save(x0::Buffer& out);

private:
	Backend* findLeastLoad(Backend::Role role, bool* allDisabled = nullptr);
	Backend* nextBackend(Backend* backend, x0::HttpRequest* r);
	void pass(x0::HttpRequest* r, RequestNotes* notes, Backend* backend);
	void enqueue(x0::HttpRequest* r);
	void updateQueueTimer();
	x0::HttpRequest* dequeue();
};
