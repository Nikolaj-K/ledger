//------------------------------------------------------------------------------
//
//   Copyright 2018 Fetch.AI Limited
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//
//------------------------------------------------------------------------------

#include "ledger/executor.hpp"
#include "core/assert.hpp"
#include "core/byte_array/encoders.hpp"
#include "core/logger.hpp"
#include "core/macros.hpp"
#include "core/mutex.hpp"

#include <algorithm>
#include <chrono>
#include <random>
#include <thread>

namespace fetch {
namespace ledger {

#if 0
std::ostream &operator<<(std::ostream &stream, Executor::lane_set_type const &lane_set)
{
  std::vector<uint16_t> elements(lane_set.size());
  std::copy(lane_set.begin(), lane_set.end(), elements.begin());
  std::sort(elements.begin(), elements.end());

  bool not_first_loop = false;
  for (auto element : elements)
  {
    if (not_first_loop)
      stream << ',';
    stream << element;
    not_first_loop = true;
  }
  return stream;
}
#endif

/**
 * Executes a given transaction across a series of lanes
 *
 * @param hash The transaction hash
 * @param slice The current block slice
 * @param lanes The affected lanes for the transaction
 * @return The status code for the operation
 */
Executor::Status Executor::Execute(tx_digest_type const &hash, std::size_t slice,
                                   lane_set_type const &lanes)
{

  fetch::logger.Info("Executing tx ", byte_array::ToBase64(hash));

  // TODO(issue 33): Add code to validate / check lane resources
  FETCH_UNUSED(slice);
  FETCH_UNUSED(lanes);

  // Get the transaction from the store (we should be able to take the
  // transaction from any of the lanes, for simplicity, however, just pick the
  // first one).
  chain::Transaction tx;
  if (!resources_->GetTransaction(hash, tx))
  {
    return Status::TX_LOOKUP_FAILURE;
  }

  Identifier identifier;
  identifier.Parse(static_cast<std::string>(tx.contract_name()));

  // Lookup the chain code associated with the transaction
  auto chain_code = chain_code_cache_.Lookup(identifier.name_space());
  if (!chain_code)
  {
    return Status::CHAIN_CODE_LOOKUP_FAILURE;
  }

  // attach the chain code to the current working context
  chain_code->Attach(*resources_);

  // Dispatch the transaction to the contract
  auto result = chain_code->DispatchTransaction(identifier.name(), tx);
  if (Contract::Status::OK != result)
  {
    return Status::CHAIN_CODE_EXEC_FAILURE;
  }

  // detach the chain code from the current context
  chain_code->Detach();

  fetch::logger.Info("Executing tx ", byte_array::ToBase64(hash), " (success)");

  return Status::SUCCESS;
}

}  // namespace ledger
}  // namespace fetch
