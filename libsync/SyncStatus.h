/*
 * @CopyRight:
 * FISCO-BCOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FISCO-BCOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FISCO-BCOS.  If not, see <http://www.gnu.org/licenses/>
 * (c) 2016-2018 fisco-dev contributors.
 */
/**
 * @brief : Sync peer
 * @author: jimmyshi
 * @date: 2018-10-16
 */
#pragma once
#include "Common.h"
#include "DownloadingBlockQueue.h"
#include "RspBlockReq.h"
#include <libblockchain/BlockChainInterface.h>
#include <libdevcore/FixedHash.h>
#include <libdevcore/Worker.h>
#include <libethcore/Exceptions.h>
#include <libnetwork/Common.h>
#include <libnetwork/Session.h>
#include <libp2p/P2PInterface.h>
#include <libtxpool/TxPoolInterface.h>
#include <map>
#include <queue>
#include <set>
#include <vector>


namespace dev
{
namespace sync
{
struct SyncStatus
{
    SyncState state = SyncState::Idle;
    PROTOCOL_ID protocolId;
    int64_t currentBlockNumber;
    int64_t knownHighestNumber;
    h256 knownLatestHash;
};

class SyncPeerStatus
{
public:
    SyncPeerStatus(PROTOCOL_ID _protocolId, NodeID const& _nodeId, int64_t _number,
        h256 const& _genesisHash, h256 const& _latestHash)
      : m_protocolId(_protocolId),
        nodeId(_nodeId),
        number(_number),
        genesisHash(_genesisHash),
        latestHash(_latestHash),
        reqQueue(_nodeId)
    {}
    SyncPeerStatus(const SyncPeerInfo& _info, PROTOCOL_ID _protocolId)
      : m_protocolId(_protocolId),
        nodeId(_info.nodeId),
        number(_info.number),
        genesisHash(_info.genesisHash),
        latestHash(_info.latestHash),
        reqQueue(_info.nodeId)
    {}

    void update(const SyncPeerInfo& _info)
    {
        nodeId = _info.nodeId;
        number = _info.number;
        genesisHash = _info.genesisHash;
        latestHash = _info.latestHash;
    }

private:
    PROTOCOL_ID m_protocolId;

public:
    NodeID nodeId;
    int64_t number;
    h256 genesisHash;
    h256 latestHash;
    DownloadRequestQueue reqQueue;
};

class SyncMasterStatus
{
public:
    SyncMasterStatus(std::shared_ptr<dev::blockchain::BlockChainInterface> _blockChain,
        PROTOCOL_ID const& _protocolId, h256 const& _genesisHash)
      : genesisHash(_genesisHash),
        knownHighestNumber(0),
        knownLatestHash(_genesisHash),
        m_protocolId(_protocolId),
        m_downloadingBlockQueue(_blockChain, _protocolId)
    {
        m_groupId = dev::eth::getGroupAndProtocol(m_protocolId).first;
    }

    SyncMasterStatus(h256 const& _genesisHash)
      : genesisHash(_genesisHash),
        knownHighestNumber(0),
        knownLatestHash(_genesisHash),
        m_protocolId(0),
        m_downloadingBlockQueue(nullptr, 0)
    {}

    bool hasPeer(NodeID const& _id);

    bool newSyncPeerStatus(SyncPeerInfo const& _info);

    void deletePeer(NodeID const& _id);

    NodeIDs peers();

    std::shared_ptr<SyncPeerStatus> peerStatus(NodeID const& _id);

    void foreachPeer(std::function<bool(std::shared_ptr<SyncPeerStatus>)> const& _f) const;

    void foreachPeerRandom(std::function<bool(std::shared_ptr<SyncPeerStatus>)> const& _f) const;

    /// Select some peers at _percent when _allow(peer)
    NodeIDs randomSelection(
        unsigned _percent, std::function<bool(std::shared_ptr<SyncPeerStatus>)> const& _allow);

    /// Select some peers at _selectSize when _allow(peer)
    NodeIDs randomSelectionSize(
        size_t _maxChosenSize, std::function<bool(std::shared_ptr<SyncPeerStatus>)> const& _allow);

    DownloadingBlockQueue& bq() { return m_downloadingBlockQueue; }

public:
    h256 genesisHash;
    mutable SharedMutex x_known;
    int64_t knownHighestNumber;
    h256 knownLatestHash;
    SyncState state = SyncState::Idle;

private:
    PROTOCOL_ID m_protocolId;
    GROUP_ID m_groupId;
    mutable SharedMutex x_peerStatus;
    std::map<NodeID, std::shared_ptr<SyncPeerStatus>> m_peersStatus;
    DownloadingBlockQueue m_downloadingBlockQueue;
};

}  // namespace sync
}  // namespace dev