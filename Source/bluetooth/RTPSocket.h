/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "Module.h"

namespace WPEFramework {

namespace Bluetooth {

    class RTPSocket : public Core::SynchronousChannelType<Core::SocketPort> {
    public:
        static constexpr uint32_t CommunicationTimeout = 500;

    public:
        class MediaPacket : public Core::IOutbound {
        private:
            struct RTPHeader {
                static constexpr uint8_t VERSION = 2;

                uint8_t octet0;
                uint8_t octet1;
                uint16_t sequence;
                uint32_t timestamp;
                uint32_t ssrc;
                uint32_t csrc[0];
            } __attribute__((packed));

        public:
            using Producer = std::function<uint32_t(uint8_t buffer[], const uint32_t max, uint32_t& consumed)>;

        public:
            MediaPacket(const MediaPacket&) = delete;
            MediaPacket& operator=(const MediaPacket&) = delete;
            MediaPacket(const uint32_t synchronisationSource, const uint8_t packetType, uint16_t sequence, const uint32_t timestamp, const uint16_t scratchPadSize, uint8_t scratchPad[])
                : _bufferSize(scratchPadSize)
                , _buffer(scratchPad)
                , _header(reinterpret_cast<RTPHeader*>(_buffer))
                , _payloadSize(_bufferSize - _headerSize)
                , _payload(_buffer + _headerSize)
                , _dataSize(0)
                , _offset(0)
            {
                ASSERT(_buffer != nullptr);
                ASSERT(_header != nullptr);
                ASSERT(_payload != nullptr);
                ASSERT(reinterpret_cast<uint8_t*>(_header) == _buffer);

                _header->octet0 = (RTPHeader::VERSION << 6);
                _header->octet1 = packetType;
                _header->ssrc = htonl(synchronisationSource);
                _header->sequence = htons(sequence);
                _header->timestamp = htonl(timestamp);
                _dataSize += sizeof(RTPHeader);
            }
            ~MediaPacket()
            {
            }

        public:
            uint32_t Ingest(const Producer& producer)
            {
                uint32_t consumed = 0;
                _dataSize += producer((_payload + _dataSize), (_payloadSize - _dataSize), consumed);
                return (consumed);
            }

        private:
            void Reload() const override
            {
                _offset = 0;
            }
            uint16_t Serialize(uint8_t stream[], const uint16_t length) const override
            {
                uint16_t result = std::min((_dataSize - _offset), static_cast<uint32_t>(length));
                if (result > 0) {
                    ::memcpy(stream, (_buffer + _offset), result);
                    _offset += result;

                    printf("RTP send [%d]: ", result); for (uint16_t index = 0; index < (16 - 1); index++) { printf("%02X:", stream[index]); } printf("%02X ...\n", stream[16 - 1]);
                }

                return (result);
            }

        private:
            uint32_t _bufferSize;
            uint8_t* _buffer;
            uint8_t _headerSize;
            RTPHeader* _header;
            uint32_t _payloadSize;
            uint8_t* _payload;
            uint32_t _dataSize;
            mutable uint32_t _offset;
        }; // class MediaPacket

    public:
        RTPSocket(const Core::NodeId& localNode, const Core::NodeId& remoteNode, const uint16_t maxMTU)
            : Core::SynchronousChannelType<Core::SocketPort>(SocketPort::SEQUENCED, localNode, remoteNode, maxMTU, maxMTU)
            , _adminLock()
        {
        }
        ~RTPSocket() = default;
        RTPSocket(const RTPSocket&) = delete;
        RTPSocket& operator=(const RTPSocket&) = delete;

    private:
        virtual void Operational() = 0;

        void StateChange() override
        {
            Core::SynchronousChannelType<Core::SocketPort>::StateChange();

            if (IsOpen() == true) {
                socklen_t len = sizeof(_connectionInfo);
                ::getsockopt(Handle(), SOL_L2CAP, L2CAP_CONNINFO, &_connectionInfo, &len);

                Operational();
            }
        }
        uint16_t Deserialize(const uint8_t dataFrame[], const uint16_t availableData) override
        {
            uint32_t result = 0;

            if (availableData == 0) {
                TRACE_L1("Unexpected data for deserialization [%d]", availableData);
            }

            return (result);
        }

    private:
        Core::CriticalSection _adminLock;
        struct l2cap_conninfo _connectionInfo;
    }; // class RTPSocket

} // namespace Bluetooth

}