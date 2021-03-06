// Copyright 2020 Artificial Intelligence Redefined <dev+cogment@ai-r.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COGMENT_ORCHESTRATOR_ORCHESTRATOR_H
#define COGMENT_ORCHESTRATOR_ORCHESTRATOR_H

#include "cogment/api/hooks.egrpc.pb.h"
#include "cogment/datalog/storage_interface.h"
#include "cogment/services/actor_service.h"
#include "cogment/services/trial_lifecycle_service.h"
#include "cogment/stub_pool.h"
#include "cogment/trial.h"
#include "cogment/trial_params.h"
#include "cogment/trial_spec.h"
#include "cogment/utils.h"

#include <atomic>
#include <unordered_map>

namespace cogment {
class Orchestrator {
  public:
  Orchestrator(Trial_spec trial_spec, cogment::TrialParams default_trial_params,
               std::shared_ptr<easy_grpc::client::Credentials> creds);
  ~Orchestrator();

  // Initialization
  void add_prehook(cogment::TrialHooks::Stub_interface* prehook);
  void set_log_exporter(std::unique_ptr<Datalog_storage_interface> le);

  // Lifecycle
  Future<std::shared_ptr<Trial>> start_trial(cogment::TrialParams params, std::string user_id);
  void end_trial(const uuids::uuid& trial_id);

  // Client API
  TrialJoinReply client_joined(TrialJoinRequest);
  ::easy_grpc::Stream_future<::cogment::TrialActionReply> bind_client(
      const uuids::uuid& trial_id, std::string& actor_name,
      ::easy_grpc::Stream_future<::cogment::TrialActionRequest> actions);

  // Services
  ActorService& actor_service() { return actor_service_; }
  TrialLifecycleService& trial_lifecycle_service() { return trial_lifecycle_service_; }

  // Lookups
  std::shared_ptr<Trial> get_trial(const uuids::uuid& trial_id) const;

  // Gets all running trials.
  std::vector<std::shared_ptr<Trial>> all_trials() const;

  // Semi-internal, rpc management related.
  easy_grpc::Completion_queue* client_queue() { return &client_queue_; }
  Channel_pool* channel_pool() { return &channel_pool_; }
  Stub_pool<cogment::EnvironmentEndpoint>* env_pool() { return &env_stubs_; }
  Stub_pool<cogment::AgentEndpoint>* agent_pool() { return &agent_stubs_; }

  const cogment::TrialParams& default_trial_params() const { return default_trial_params_; };

  const Trial_spec& get_trial_spec() const { return trial_spec_; }

  private:
  // Configuration
  Trial_spec trial_spec_;
  cogment::TrialParams default_trial_params_;

  // Currently existing Trials
  mutable std::mutex trials_mutex_;
  std::unordered_map<uuids::uuid, std::shared_ptr<Trial>> trials_;

  // List of trial pre-hooks to invoke before actually launching trials
  std::vector<cogment::TrialHooks::Stub_interface*> prehooks_;

  // Send trial data to this destination.
  std::unique_ptr<Datalog_storage_interface> log_exporter_;

  // Completion queue for handling requests returning from env/agent/hooks
  easy_grpc::Completion_queue client_queue_;
  Channel_pool channel_pool_;

  Stub_pool<cogment::EnvironmentEndpoint> env_stubs_;
  Stub_pool<cogment::AgentEndpoint> agent_stubs_;

  ActorService actor_service_;
  TrialLifecycleService trial_lifecycle_service_;

  std::atomic<int> garbage_collection_countdown_;
  void check_garbage_collection_();
  void perform_garbage_collection_();

  Future<cogment::PreTrialContext> perform_pre_hooks_(cogment::PreTrialContext ctx, const std::string& trial_id);
};
}  // namespace cogment
#endif