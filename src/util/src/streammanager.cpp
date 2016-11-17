///
//
// LibSourcey
// Copyright (c) 2005, Sourcey <http://sourcey.com>
//
// SPDX-License-Identifier:	LGPL-2.1+
//
/// @addtogroup util
/// @{


#include "scy/util/streammanager.h"
#include "scy/logger.h"


using namespace std;


namespace scy {


StreamManager::StreamManager(bool freeClosedStreams) :
    _freeClosedStreams(freeClosedStreams)
{
}


StreamManager::~StreamManager()
{
    DebugL << "Destroy" << endl;
    closeAll();
}


void StreamManager::closeAll()
{
    Mutex::ScopedLock lock(_mutex);

    DebugL << "Close all streams: " << _map.size() << endl;
    StreamManager::Map::iterator it = _map.begin();
    StreamManager::Map::iterator it2;
    while (it != _map.end()) {
        it2 = it++;
        (*it2).second->StateChange -= slot(this, &StreamManager::onStreamStateChange);
        (*it2).second->close();
        if (_freeClosedStreams) {
            StreamManager::Deleter func;
            func((*it2).second);
        }
        _map.erase(it2);
    }
}


bool StreamManager::addStream(PacketStream* stream, bool whiny)
{
    assert(stream);
    assert(!stream->name().empty());
    return Manager::add(stream->name(), stream, whiny);
}


PacketStream* StreamManager::getStream(const std::string& name, bool whiny)
{
    return Manager::get(name, whiny);
}


bool StreamManager::closeStream(const std::string& name, bool whiny)
{
    assert(!name.empty());

    DebugL << "Close stream: " << name << endl;
    PacketStream* stream = get(name, whiny);
    if (stream) {
        stream->close();
        return true;
    }
    return false;
}


PacketStream* StreamManager::getDafaultStream()
{
    Mutex::ScopedLock lock(_mutex);

    // Returns the first stream or NULL.
    if (!_map.empty()) {
        StreamManager::Map::const_iterator it = _map.begin();
        return it->second;
    }

    return NULL;
}


void StreamManager::onAdd(PacketStream* stream)
{
    // Stream name can't be empty
    assert(!stream->name().empty());

    // Receive callbacks after all other listeners
    // so we can delete the stream when it closes.
    DebugL << "stream added: " << stream->name() << endl;
    stream->StateChange += slot(this, &StreamManager::onStreamStateChange, -1);
}


void StreamManager::onRemove(PacketStream* stream)
{
    DebugL << "stream removed: " << stream->name() << endl;
    stream->StateChange -= slot(this, &StreamManager::onStreamStateChange);
}


void StreamManager::onStreamStateChange(void* sender, PacketStreamState& state, const PacketStreamState&)
{
    DebugL << "Stream state change: " << state << endl;

    // Cantch stream closed state and free it if necessary
    if (state.equals(PacketStreamState::Closed)) {
        auto stream = reinterpret_cast<PacketStream*>(sender);
        stream->StateChange -= slot(this, &StreamManager::onStreamStateChange);
        bool success = false;
        if (_freeClosedStreams) {
            DebugL << "On stream close: freeing: " << stream->name() << endl;
            success = Manager::free(stream->name());
        } else {
            DebugL << "On stream close: removing: " << stream->name() << endl;
            success = !!Manager::remove(stream->name());
        }
        if (!success) {
            WarnL << "Cannot remove stream: " << stream->name() << endl;
            assert(0);
        }
    }
}


StreamManager::Map StreamManager::streams() const
{
    Mutex::ScopedLock lock(_mutex);
    return _map;
}


void StreamManager::print(std::ostream& os) const
{
    Mutex::ScopedLock lock(_mutex);

    os << "StreamManager[";
    for (StreamManager::Map::const_iterator it = _map.begin(); it != _map.end(); ++it) {
        os << "\n\t" << it->second << ": " << it->first;
    }
    os << "\n]";
}


} // namespace scy
