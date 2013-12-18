#include <x0/flow/vm/NativeCallback.h>
#include <x0/IPAddress.h>
#include <x0/Cidr.h>
#include <x0/RegExp.h>

namespace x0 {
namespace FlowVM {

// constructs a handler callback
NativeCallback::NativeCallback(Runtime* runtime, const std::string& _name) :
    runtime_(runtime),
    isHandler_(true),
    function_(),
    signature_()
{
    signature_.setName(_name);
    signature_.setReturnType(FlowType::Boolean);
}

// constructs a function callback
NativeCallback::NativeCallback(Runtime* runtime, const std::string& _name, FlowType _returnType) :
    runtime_(runtime),
    isHandler_(false),
    function_(),
    signature_()
{
    signature_.setName(_name);
    signature_.setReturnType(_returnType);
}

NativeCallback::~NativeCallback()
{
    for (size_t i = 0, e = defaults_.size(); i != e; ++i) {
        FlowType type = signature_.args()[i];
        switch (type) {
            case FlowType::Boolean:
                break;
            case FlowType::Number:
                break;
            case FlowType::String:
                delete (std::string*) defaults_[i];
                break;
            case FlowType::IPAddress:
                delete (IPAddress*) defaults_[i];
                break;
            case FlowType::Cidr:
                delete (Cidr*) defaults_[i];
                break;
            case FlowType::RegExp:
                delete (RegExp*) defaults_[i];
                break;
            case FlowType::Handler:
            case FlowType::Array:
            default:
                break;
        }
    }
}

bool NativeCallback::isHandler() const
{
    return isHandler_;
}

const std::string NativeCallback::name() const
{
    return signature_.name();
}

const Signature& NativeCallback::signature() const
{
    return signature_;
}

void NativeCallback::invoke(Params& args) const
{
    function_(args);
}

} // namespace FlowVM
} // namespace x0
