// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include <x0d/Config.h>

#include <xzero/net/Server.h>
#include <xzero/Buffer.h>
#include <xzero/MimeTypes.h>
#include <xzero/io/LocalFileRepository.h>
#include <xzero/Signal.h>
#include <xzero/UnixTime.h>
#include <xzero/Duration.h>
#include <xzero/net/InetConnector.h>
#include <xzero/executor/ThreadedExecutor.h>
#include <xzero/executor/NativeScheduler.h>
#include <xzero/http/HttpFileHandler.h>
#include <xzero/http/http1/ConnectionFactory.h>
#include <xzero-flow/AST.h>
#include <xzero-flow/ir/IRProgram.h>
#include <xzero-flow/vm/Runtime.h>
#include <xzero-flow/vm/Program.h>
#include <xzero-flow/vm/Handler.h>
#include <xzero-flow/vm/NativeCallback.h>
#include <list>
#include <vector>
#include <string>
#include <memory>
#include <cstdint>
#include <iosfwd>

namespace xzero {
  class IPAddress;
  class Connection;
  class Connector;
  class EventLoop;

  namespace http {
    class HttpRequest;
    class HttpResponse;
  }

  namespace flow {
    class CallExpr;
    class IRGenerator;
  }
}

namespace x0d {

class XzeroModule;
class XzeroEventHandler;

class XzeroDaemon : public xzero::flow::vm::Runtime {
 public:
  typedef xzero::Signal<void(xzero::Connection*)> ConnectionHook;
  typedef xzero::Signal<void(xzero::http::HttpRequest*, xzero::http::HttpResponse*)> RequestHook;
  typedef xzero::Signal<void()> CycleLogsHook;

 public:
  XzeroDaemon();
  ~XzeroDaemon();

  void setOptimizationLevel(int level) { optimizationLevel_ = level; }

  // {{{ config management
  std::shared_ptr<xzero::flow::vm::Program> loadConfigFile(
      const std::string& configFileName);
  std::shared_ptr<xzero::flow::vm::Program> loadConfigFile(
      const std::string& configFileName,
      bool printAST, bool printIR, bool printTC);
  std::shared_ptr<xzero::flow::vm::Program> loadConfigEasy(
      const std::string& docroot, int port);
  std::shared_ptr<xzero::flow::vm::Program> loadConfigStream(
      std::unique_ptr<std::istream>&& is, const std::string& name,
      bool printAST, bool printIR, bool printTC);
  void reloadConfiguration();
  bool applyConfiguration(std::shared_ptr<xzero::flow::vm::Program> program);
  // }}}

  void run();
  void terminate();

  xzero::Executor* selectClientExecutor();

  template<typename T>
  void setupConnector(const xzero::IPAddress& ipaddr, int port,
                      int backlog, int multiAccept,
                      bool reuseAddr, bool reusePort,
                      std::function<void(T*)> connectorVisitor);

  template<typename T>
  T* doSetupConnector(xzero::Executor* executor,
                      xzero::InetConnector::ExecutorSelector clientExecutorSelector,
                      const xzero::IPAddress& ipaddr, int port,
                      int backlog, int multiAccept,
                      bool reuseAddr, bool reusePort);

  template <typename... ArgTypes>
  xzero::flow::vm::NativeCallback& setupFunction(
      const std::string& name, xzero::flow::vm::NativeCallback::Functor cb,
      ArgTypes... argTypes);

  template <typename... ArgTypes>
  xzero::flow::vm::NativeCallback& sharedFunction(
      const std::string& name, xzero::flow::vm::NativeCallback::Functor cb,
      ArgTypes... argTypes);

  template <typename... ArgTypes>
  xzero::flow::vm::NativeCallback& mainFunction(
      const std::string& name, xzero::flow::vm::NativeCallback::Functor cb,
      ArgTypes... argTypes);

  template <typename... ArgTypes>
  xzero::flow::vm::NativeCallback& mainHandler(
      const std::string& name, xzero::flow::vm::NativeCallback::Functor cb,
      ArgTypes... argTypes);

 public:
  // flow::vm::Runtime overrides
  virtual bool import(
      const std::string& name,
      const std::string& path,
      std::vector<xzero::flow::vm::NativeCallback*>* builtins);

 private:
  void validateConfig(xzero::flow::Unit* unit);
  void validateContext(const std::string& entrypointHandlerName,
                       const std::vector<std::string>& api,
                       xzero::flow::Unit* unit);
  void stopThreads();
  void startThreads();

  void handleRequest(xzero::http::HttpRequest* request, xzero::http::HttpResponse* response);

 public: // signals raised on request in order
  //! This hook is invoked once a new client has connected.
  ConnectionHook onConnectionOpen;

  //! is called at the very beginning of a request.
  RequestHook onPreProcess;

  //! gets invoked right before serializing headers.
  RequestHook onPostProcess;

  //! this hook is invoked once the request has been fully served to the client.
  RequestHook onRequestDone;

  //! Hook that is invoked when a connection gets closed.
  ConnectionHook onConnectionClose;

  //! Hook that is invoked whenever a cycle-the-logfiles is being triggered.
  CycleLogsHook onCycleLogs;

  xzero::MimeTypes& mimetypes() noexcept { return mimetypes_; }
  xzero::LocalFileRepository& vfs() noexcept { return vfs_; }
  xzero::http::HttpFileHandler& fileHandler() noexcept { return fileHandler_; }

  Config& config() const { return *config_; }
  xzero::Server* server() const { return server_.get(); }

  template<typename T>
  T* loadModule();

 private:
  std::unique_ptr<Config> createDefaultConfig();
  void patchProgramIR(xzero::flow::IRProgram* program,
                      xzero::flow::IRGenerator* irgen);
  void postConfig();
  std::unique_ptr<xzero::EventLoop> createEventLoop();
  void runOneThread(int index);
  void setThreadAffinity(int cpu, int workerId);

 private:
  unsigned generation_;                  //!< process generation number
  xzero::UnixTime startupTime_;          //!< process startup time
  std::atomic<bool> terminate_;

  std::unique_ptr<XzeroEventHandler> eventHandler_;

  xzero::MimeTypes mimetypes_;
  xzero::LocalFileRepository vfs_;

  off_t lastWorker_;                          //!< offset to the last elected worker
  xzero::ThreadedExecutor threadedExecutor_;  //!< non-main worker executor
  std::vector<std::unique_ptr<xzero::EventLoop>> eventLoops_; //!< one for each thread
  std::list<std::unique_ptr<XzeroModule>> modules_; //!< list of loaded modules
  std::unique_ptr<xzero::Server> server_;     //!< (HTTP) server instance

  // Flow configuration
  std::shared_ptr<xzero::flow::vm::Program> program_; // kept to preserve strong reference count
  std::shared_ptr<xzero::flow::vm::Handler> main_;
  std::vector<std::string> setupApi_;
  std::vector<std::string> mainApi_;
  int optimizationLevel_;

  // HTTP
  xzero::http::HttpFileHandler fileHandler_;
  std::shared_ptr<xzero::http::http1::ConnectionFactory> http1_;

  // setup phase
  std::string configFilePath_;
  std::unique_ptr<Config> config_;

  friend class CoreModule;
};

// {{{ inlines
template <typename... ArgTypes>
inline xzero::flow::vm::NativeCallback& XzeroDaemon::setupFunction(
    const std::string& name, xzero::flow::vm::NativeCallback::Functor cb,
    ArgTypes... argTypes) {
  setupApi_.push_back(name);
  return registerFunction(name, xzero::flow::FlowType::Void).bind(cb).params(
      argTypes...);
}

template <typename... ArgTypes>
inline xzero::flow::vm::NativeCallback& XzeroDaemon::sharedFunction(
    const std::string& name, xzero::flow::vm::NativeCallback::Functor cb,
    ArgTypes... argTypes) {
  setupApi_.push_back(name);
  mainApi_.push_back(name);
  return registerFunction(name, xzero::flow::FlowType::Void).bind(cb).params(
      argTypes...);
}

template <typename... ArgTypes>
inline xzero::flow::vm::NativeCallback& XzeroDaemon::mainFunction(
    const std::string& name, xzero::flow::vm::NativeCallback::Functor cb,
    ArgTypes... argTypes) {
  mainApi_.push_back(name);
  return registerFunction(name, xzero::flow::FlowType::Void).bind(cb).params(
      argTypes...);
}

template <typename... ArgTypes>
inline xzero::flow::vm::NativeCallback& XzeroDaemon::mainHandler(
    const std::string& name, xzero::flow::vm::NativeCallback::Functor cb,
    ArgTypes... argTypes) {
  mainApi_.push_back(name);
  return registerHandler(name).bind(cb).params(argTypes...);
}

template<typename T>
inline T* XzeroDaemon::loadModule() {
  modules_.emplace_back(new T(this));
  return static_cast<T*>(modules_.back().get());
}

// }}}

} // namespace x0d
