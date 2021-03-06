#pragma once
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

// This represents the API to the network test
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <set>
#include <stdlib.h>
#include <utility>
#include <vector>

#include "core/byte_array/const_byte_array.hpp"
#include "core/logger.hpp"
#include "network_classes.hpp"
#include "node_directory.hpp"

#include "ledger/chain/consensus/dummy_miner.hpp"
#include "ledger/chain/main_chain.hpp"

namespace fetch {
namespace network_mine_test {

class MineNodeBasic
{

  // Main chain
  using block_type = chain::MainChain::block_type;
  using block_hash = chain::MainChain::block_hash;
  using body_type  = chain::MainChain::block_type::body_type;
  using miner      = fetch::chain::consensus::DummyMiner;

public:
  explicit MineNodeBasic(network::NetworkManager tm, uint64_t minerNumber)
    : nodeDirectory_{tm}, minerNumber_{minerNumber}
  {}

  MineNodeBasic(MineNodeBasic &rhs)  = delete;
  MineNodeBasic(MineNodeBasic &&rhs) = delete;
  MineNodeBasic operator=(MineNodeBasic &rhs) = delete;
  MineNodeBasic operator=(MineNodeBasic &&rhs) = delete;

  ~MineNodeBasic() {}

  ///////////////////////////////////////////////////////////
  // RPC calls
  void ReceiveNewHeader(block_type &block)
  {
    block.UpdateDigest();

    // Verify the block
    block.proof().SetTarget(target_);

    if (!block.proof()())
    {
      fetch::logger.Warn("Received not verified");
    }
    else
    {
      mainChain.AddBlock(block);

      // The main chain will set whether that block was loose. If it was, try
      // and walk down until it touches the main chain
      if (block.loose())
      {
        std::thread{[this, block] {
          block_type copy = block;
          this->SyncBlock(copy);
        }}
            .detach();
      }
    }
  }

  // Called async. when we see a new block that's loose, work to connect it to
  // the main chain
  void SyncBlock(block_type &block)
  {
    block_type walkBlock;
    block_hash hash = block.body().previous_hash;

    do
    {
      bool success = nodeDirectory_.GetHeader(hash, walkBlock);
      if (!success) break;

      walkBlock.UpdateDigest();  // critical we update the hash after transmission
      hash = walkBlock.body().previous_hash;

    } while (mainChain.AddBlock(walkBlock));
  }

  // Nodes will provide each other with headers
  std::pair<bool, block_type> ProvideHeader(block_hash hash)
  {
    block_type block;
    bool       success = mainChain.Get(hash, block);

    return std::make_pair(success, block);
  }

  ///////////////////////////////////////////////////////////
  // HTTP calls for setup
  void AddEndpoint(const network_benchmark::Endpoint &endpoint)
  {
    LOG_STACK_TRACE_POINT;
    std::lock_guard<std::mutex> mlock(mutex_);
    fetch::logger.Info("Adding endpoint");
    nodeDirectory_.AddEndpoint(endpoint);
  }

  void reset()
  {
    fetch::logger.Info("Resetting miner");
    mainChain.reset();
    stopped_ = true;
  }

  ///////////////////////////////////////////////////////////
  // Mining loop
  void startMining()
  {
    auto closure = [this] {
      // Loop code
      while (!stopped_)
      {
        // Get heaviest block
        auto &block = mainChain.HeaviestBlock();

        // Create another block sequential to previous
        block_type nextBlock;
        body_type  nextBody;
        nextBody.block_number  = block.body().block_number + 1;
        nextBody.previous_hash = block.hash();
        nextBody.miner_number  = minerNumber_;

        nextBlock.SetBody(nextBody);
        nextBlock.UpdateDigest();

        // Mine the block
        nextBlock.proof().SetTarget(target_);
        miner::Mine(nextBlock);

        if (stopped_)
        {
          break;
        }

        // Add the block
        mainChain.AddBlock(nextBlock);

        // Pass the block to other miners
        nodeDirectory_.PushBlock(nextBlock);
      }
    };

    std::thread{closure}.detach();
  }

  void stopMining()
  {
    fetch::logger.Info("Stopping mining");
    stopped_ = true;
  }

  ///////////////////////////////////////////////////////////////
  // HTTP functions to check that synchronisation was successful
  std::vector<block_type> HeaviestChain() { return mainChain.HeaviestChain(); }

  std::pair<block_type, std::vector<std::vector<block_type>>> AllChain()
  {
    return mainChain.AllChain();
  }

private:
  network_benchmark::NodeDirectory nodeDirectory_;  // Manage connections to other nodes
  fetch::mutex::Mutex              mutex_;
  bool                             stopped_{false};
  std::size_t                      target_ = 16;  // 16 = roughly one block every 0.18s
  uint64_t                         minerNumber_{1};

  chain::MainChain mainChain{uint32_t(minerNumber_)};
};
}  // namespace network_mine_test
}  // namespace fetch
