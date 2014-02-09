/* Copyright (C) 2013-2014 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of FreeFT.
 */

#include "sys/host.h"
#include <algorithm>

#define LOGGING

namespace net {

	namespace {

#ifdef LOGGING
		struct LogInfo {
			bool is_sending;
			Address address;
			int packet_id;
			vector<pair<int, int>> chunks;
			vector<int> acks;
		} static s_log_info;
#endif

		void logBegin(bool is_sending, const Address &address, int packet_id) {
#ifdef LOGGING
			s_log_info.is_sending = is_sending;
			s_log_info.address = address;
			s_log_info.packet_id = packet_id;
			s_log_info.chunks.clear();
			s_log_info.acks.clear();
#endif
		}

		void logChunk(ChunkType chunk_type, int id) {
#ifdef LOGGING
			s_log_info.chunks.push_back(make_pair(id, (int)chunk_type));
#endif
		}

		void logAck(int packet_id) {
#ifdef LOGGING
			s_log_info.acks.push_back(packet_id);
#endif
		}

		void logEnd(int packet_size) {
#ifdef LOGGING
			if(s_log_info.chunks.empty())
				return;

			printf("%s(size:%d id:%d)[ ", s_log_info.is_sending? "OUT" : " IN", packet_size, s_log_info.packet_id);
			for(int n = 0; n < (int)s_log_info.chunks.size(); n++)
				printf("%d:%d ", s_log_info.chunks[n].first, s_log_info.chunks[n].second);
	/*		if(!s_log_info.acks.empty()) {
				printf("ACK: ");
				for(int n = 0; n < (int)s_log_info.acks.size(); n++)
					printf("%d ", s_log_info.acks[n]);
			} */

			printf("]\n");
#endif
		}

	}


	Chunk::Chunk(const char *data, int data_size, ChunkType type, int chunk_id, int channel_id_)
		:data(data, data_size), chunk_id(chunk_id), type(type) {
			DASSERT(channel_id_ >= 0 && channel_id_ <= 255);
			channel_id = channel_id_;
		}

	InChunk::InChunk(const Chunk *chunk) :Stream(true), m_chunk(chunk) {
		if(chunk) {
			m_size = chunk->data.size();
			m_pos = 0;
		}
	}

	void InChunk::v_load(void *ptr, int count) {
		DASSERT(ptr && count <= m_size - m_pos && m_chunk);
		memcpy(ptr, m_chunk->data.data() + m_pos, count);
		m_pos += count;
	}

#define INSERT(list, id) listInsert<Chunk, &Chunk::node>(m_chunks, list, id)
#define REMOVE(list, id) listRemove<Chunk, &Chunk::node>(m_chunks, list, id)

	RemoteHost::RemoteHost(const Address &address, int max_bytes_per_frame, int current_id, int remote_id)
		:m_address(address), m_max_bpf(max_bytes_per_frame), m_out_packet_id(-1), m_in_packet_id(-1),
		 m_socket(nullptr), m_current_id(current_id), m_remote_id(remote_id), m_is_verified(false),
		 m_last_timestamp(0) {
		DASSERT(address.isValid());
		m_channels.resize(max_channels);
	}

	void RemoteHost::sendPacket() {
		logEnd(m_out_packet.size());
		m_socket->send(m_out_packet.m_data, m_out_packet.size(), m_address);
	}

	void RemoteHost::newPacket(bool is_first) {
		int packet_idx = m_packets.alloc();
		Packet &packet = m_packets[packet_idx];

		m_out_packet_id++;
		m_out_packet = OutPacket(m_out_packet_id, m_current_id, m_remote_id, is_first?PacketInfo::flag_first : 0);
		packet.packet_id = m_out_packet_id;
		m_packet_idx = packet_idx;
		
		logBegin(true, m_address, m_out_packet_id);
	}

	void RemoteHost::enqueChunk(const TempPacket &data, ChunkType type, int channel_id) {
		enqueChunk(data.data(), data.size(), type, channel_id);
	}

	void RemoteHost::enqueChunk(const char *data, int data_size, ChunkType type, int channel_id) {
	//	DASSERT(isSending());
		DASSERT(channel_id >= 0 && channel_id < (int)m_channels.size());
		Channel &channel = m_channels[channel_id];

		int chunk_idx = allocChunk();
		m_chunks[chunk_idx] = std::move(Chunk(data, data_size, type, channel.last_chunk_id++, channel_id));
		INSERT(channel.chunks, chunk_idx);
	}

	bool RemoteHost::enqueUChunk(const TempPacket &data, ChunkType type, int identifier, int channel_id) {
		return enqueUChunk(data.data(), data.size(), type, identifier, channel_id);
	}

	bool RemoteHost::enqueUChunk(const char *data, int data_size, ChunkType type, int identifier, int channel_id) {
		if(!canFit(data_size))
			return false;

		sendChunks(channel_id - 1);

		DASSERT(isSending());
		DASSERT(channel_id >= 0 && channel_id < (int)m_channels.size());
		Channel &channel = m_channels[channel_id];

		int chunk_idx = allocUChunk();
		UChunk &chunk = m_uchunks[chunk_idx];
		chunk.chunk_id = identifier;
		chunk.channel_id = channel_id;
		return sendUChunk(chunk_idx, data, data_size, type);
	}
		
	void RemoteHost::sendChunks(int max_channel) {
		//TODO: this function shouldnt be called every time we try to send an unreliable packet
		DASSERT(max_channel < (int)m_channels.size());

		bool finished = false;

		for(int c = 0; c <= max_channel && !finished; c++) {
			Channel &channel = m_channels[c];

			int chunk_idx = channel.chunks.head;
			while(chunk_idx != -1) {
				Chunk &chunk = m_chunks[chunk_idx];
				int next_idx = chunk.node.next;

				if(!sendChunk(chunk_idx)) {
					finished = true;
					break;
				}

				chunk_idx = next_idx;
			}
		}
	}
		
	bool RemoteHost::sendChunk(int chunk_idx) {
		DASSERT(chunk_idx >= 0 && chunk_idx < (int)m_chunks.size());
		Chunk &chunk = m_chunks[chunk_idx];

		if(sendChunk(chunk.data.data(), chunk.data.size(), chunk.type, chunk.chunk_id, chunk.channel_id, true)) {
			REMOVE(m_channels[chunk.channel_id].chunks, chunk_idx);
			INSERT(m_packets[m_packet_idx].chunks, chunk_idx);
			chunk.packet_id = m_packets[m_packet_idx].packet_id;
			return true;
		}

		return false;
	}

	bool RemoteHost::sendUChunk(int chunk_idx, const char *data, int data_size, ChunkType type) {
		DASSERT(chunk_idx >= 0 && chunk_idx < (int)m_uchunks.size());
		DASSERT(type != ChunkType::ack && type != ChunkType::invalid && type != ChunkType::multiple_chunks);

		UChunk &chunk = m_uchunks[chunk_idx];

		if(sendChunk(data, data_size, type, chunk.chunk_id, chunk.channel_id, false)) {
			listInsert<UChunk, &UChunk::node>(m_uchunks, m_packets[m_packet_idx].uchunks, chunk_idx);
			chunk.packet_id = m_packets[m_packet_idx].packet_id;
			return true;
		}

		return false;
	}

	void RemoteHost::beginSending(Socket *socket) {
		DASSERT(socket && !isSending());
		m_socket = socket;

		newPacket(true);

		int num_acks = min((int)max_ack_per_frame, (int)m_out_acks.size());
		for(int n = 0; n < num_acks; n++)
			logAck(m_out_acks[n]);

		//TODO: send ack range
		m_out_packet.encodeInt(num_acks);
		if(num_acks) {
			m_out_packet << m_out_acks[0];
			for(int i = 1; i < num_acks;) {
				int diff = m_out_acks[i] - m_out_acks[i - 1];
				int count = 1;
				while(i + count < num_acks && int(m_out_acks[i + count]) == int(m_out_acks[i + count - 1]) + 1)
					count++;

				m_out_packet.encodeInt(diff * 2 + (count > 1? 1 : 0));
				if(count > 1)
					m_out_packet.encodeInt(count);
				i += count;
			}

			if(m_out_acks.size() > max_unacked_packets) {
				int drop_acks = (int)m_out_acks.size() - max_unacked_packets;
				m_out_acks.erase(m_out_acks.begin(), m_out_acks.begin() + drop_acks);
			}
		}

		m_bytes_left = m_max_bpf - m_out_packet.pos();
	}
	
	bool RemoteHost::sendChunk(const char *data, int data_size, ChunkType type, int chunk_id, int channel_id, bool is_reliable) {
		if(!canFit(data_size))
			return false;

		if(m_out_packet.spaceLeft() < estimateSize(data_size)) {
			sendPacket();
			newPacket(false);
		}

		logChunk(type, chunk_id);

		int prev_pos = m_out_packet.pos();
		m_out_packet << type;
		m_out_packet.encodeInt(chunk_id);
		m_out_packet.encodeInt(is_reliable? 2 * channel_id + 1 : 2 * channel_id);
		m_out_packet.encodeInt(data_size);
		m_out_packet.saveData(data, data_size);
		m_bytes_left -= m_out_packet.pos() - prev_pos;

		return true;
	}

	void RemoteHost::finishSending() {
		DASSERT(isSending());
		sendChunks((int)m_channels.size() - 1);
		if(m_out_packet.pos() > PacketInfo::header_size)
			sendPacket();
		m_socket = nullptr;
	}

	void RemoteHost::beginReceiving() {
		m_current_ichunk_indices.clear();
		m_in_acks.clear();
	}

	struct OrderIChunks {
		OrderIChunks(const vector<Chunk> &chunks) :chunks(chunks) { }

		bool operator()(int c1, int c2) {
			const Chunk &chunk1 = chunks[c1];
			const Chunk &chunk2 = chunks[c2];

			return chunk1.channel_id == chunk2.channel_id?
				chunk1.chunk_id < chunk2.chunk_id :
				chunk1.channel_id < chunk2.channel_id;
		}

		const vector<Chunk> &chunks;
	};

	void RemoteHost::receive(InPacket &packet, int timestamp) {
		m_last_timestamp = timestamp;

		if(m_remote_id == -1)
			m_remote_id = packet.currentId();

		m_out_acks.push_back(packet.packetId());
		int idx = m_in_packets.alloc();
		m_in_packets[idx] = packet;
	}

	void RemoteHost::handlePacket(InPacket &packet) {
		if(packet.packetId() < m_in_packet_id)
			return;
		m_in_packet_id = packet.packetId();

		logBegin(false, m_address, packet.packetId()); 

		if(packet.flags() & PacketInfo::flag_first) {
			int num_acks = packet.decodeInt();
			if(num_acks < 0 || num_acks > max_ack_per_frame)
				goto ERROR;

			if(num_acks) {
				int offset = (int)m_in_acks.size();
				m_in_acks.resize(offset + num_acks);
				packet >> m_in_acks[offset];
				for(int i = 1; i < num_acks;) {
					int diff, count;
					diff = packet.decodeInt();
					if(diff & 1)
						count = packet.decodeInt();
					else
						count = 1;
					diff /= 2;

					m_in_acks[offset + i] = SeqNumber(int(m_in_acks[offset + i - 1]) + diff);
					for(int j = 1; j < count; j++)
						m_in_acks[offset + i + j] = SeqNumber(m_in_acks[offset + i + j - 1] + 1);
					i += count;
				}
			
				for(int n = 0; n < num_acks; n++)
					logAck(m_in_acks[offset + n]);
			}

		}


		while(!packet.end()) {
			ChunkType type;
			packet >> type;

			int chunk_id = packet.decodeInt();
			int channel_id = packet.decodeInt();
			bool is_reliable = channel_id & 1;
			channel_id >>= 1;
			
			logChunk(type, chunk_id);

			int data_size = packet.decodeInt();

			if(data_size < 0 || data_size > PacketInfo::max_size ||
				channel_id < 0 || channel_id >= (int)m_channels.size())
				goto ERROR;

			char data[PacketInfo::max_size];
			packet.loadData(data, data_size);

			int chunk_idx = allocChunk();
			Chunk &new_chunk = m_chunks[chunk_idx];
			new_chunk = std::move(Chunk(data, data_size, type, chunk_id, channel_id));

			if(is_reliable) {
				m_current_ichunk_indices.push_back(chunk_idx);
			}
			else
				INSERT(m_out_ichunks, chunk_idx);
		}	

ERROR:;
	  //TODO: record error stats and do something if too high?
	  logEnd(packet.size());
	}

	void RemoteHost::finishReceiving() {
		int last_first = -1;

		for(int idx = m_in_packets.head(); idx != -1; idx = m_in_packets.next(idx))
			if(m_in_packets[idx].flags() & PacketInfo::flag_first) {
				if(idx == m_in_packets.tail())
					break;
				last_first = idx;
				break;
			}
 
		for(int idx = m_in_packets.tail(); idx != last_first; ) {
			int next = m_in_packets.prev(idx);
			handlePacket(m_in_packets[idx]);
			m_in_packets.free(idx);
			idx = next;
		}

		vector<int> temp(m_ichunk_indices.size() + m_current_ichunk_indices.size());

		std::sort(m_current_ichunk_indices.begin(), m_current_ichunk_indices.end(), OrderIChunks(m_chunks));
		std::merge(m_ichunk_indices.begin(), m_ichunk_indices.end(),
					m_current_ichunk_indices.begin(), m_current_ichunk_indices.end(), temp.begin());
		temp.swap(m_ichunk_indices);
		m_current_ichunk_indices.clear();

		for(int n = 0; n < (int)m_ichunk_indices.size(); n++) {
			int idx = m_ichunk_indices[n];
			Chunk &chunk = m_chunks[idx];
			Channel &channel = m_channels[chunk.channel_id];

			if(channel.last_ichunk_id == chunk.chunk_id) {
				channel.last_ichunk_id++;
				INSERT(m_out_ichunks, idx);
				m_ichunk_indices[n] = -1;
			}
		}

		m_ichunk_indices.resize(
			std::remove(m_ichunk_indices.begin(), m_ichunk_indices.end(), -1) - m_ichunk_indices.begin());


		//TODO: make client verification more secure
		
		//TODO: unsafe, theoretically a cycle is possible 
		std::stable_sort(m_in_acks.begin(), m_in_acks.end());
		m_in_acks.resize(std::unique(m_in_acks.begin(), m_in_acks.end()) - m_in_acks.begin());

		int packet_idx = m_packets.tail();
		int ack_idx = 0;
		while(packet_idx != -1 && ack_idx < (int)m_in_acks.size()) {
			SeqNumber acked = m_in_acks[ack_idx];
			Packet &packet = m_packets[packet_idx];

			if(acked == packet.packet_id) {
				int next_idx = m_packets.prev(packet_idx);
				acceptPacket(packet_idx);
				packet_idx = next_idx;
				ack_idx++;
			}
			else if(acked > packet.packet_id) {
				int next_idx = m_packets.prev(packet_idx);
				resendPacket(packet_idx);
				packet_idx = next_idx;
			}
			else
				ack_idx++;
		}

		while(m_packets.listSize() > max_unacked_packets)
			resendPacket(m_packets.tail());	
	}

	void RemoteHost::acceptPacket(int packet_idx) {
		Packet &packet = m_packets[packet_idx];
		int chunk_idx = packet.chunks.head;
		while(chunk_idx != -1) {
			Chunk &chunk = m_chunks[chunk_idx];
			int next_idx = chunk.node.next;
			chunk.node = ListNode();
			INSERT(m_free_chunks, chunk_idx);
			chunk_idx = next_idx;
		}
		chunk_idx = packet.uchunks.head;
		while(chunk_idx != -1) {
			UChunk &chunk = m_uchunks[chunk_idx];
			int next_idx = chunk.node.next;
			chunk.node = ListNode();
			listInsert<UChunk, &UChunk::node>(m_uchunks, m_free_uchunks, chunk_idx);
			chunk_idx = next_idx;
		}

		packet.uchunks = List();
		packet.chunks = List();
		m_packets.free(packet_idx);
	}
	
	void RemoteHost::resendPacket(int packet_idx) {
		Packet &packet = m_packets[packet_idx];
		int chunk_idx = packet.chunks.head;
		while(chunk_idx != -1) {
			Chunk &chunk = m_chunks[chunk_idx];
			int next_idx = chunk.node.next;
			chunk.node = ListNode();
			INSERT(m_channels[chunk.channel_id].chunks, chunk_idx);
			chunk_idx = next_idx;
		}
		chunk_idx = packet.uchunks.head;
		while(chunk_idx != -1) {
			UChunk &chunk = m_uchunks[chunk_idx];
			int next_idx = chunk.node.next;
			chunk.node = ListNode();
			listInsert<UChunk, &UChunk::node>(m_uchunks, m_free_uchunks, chunk_idx);
			m_lost_uchunk_indices.push_back(chunk.chunk_id);
			chunk_idx = next_idx;
		}

		packet.uchunks = List();
		packet.chunks = List();
		m_packets.free(packet_idx);
	}

	const Chunk *RemoteHost::getIChunk() {
		if(m_out_ichunks.isEmpty())
			return nullptr;

		Chunk *out = &m_chunks[m_out_ichunks.head];
		REMOVE(m_out_ichunks, m_out_ichunks.head);
		return out;
	}

	bool RemoteHost::canFit(int data_size) const {
		return estimateSize(data_size) <= m_bytes_left;
	}
		
	int RemoteHost::estimateSize(int data_size) const {
		return data_size + 8;
	}
	
	LocalHost::LocalHost(const net::Address &address)
		:m_socket(address), m_current_id(-1), m_unverified_count(0), m_timestamp(0) {
		}

	void LocalHost::beginFrame() {
		receive();
	}

	void LocalHost::finishFrame() {
		m_timestamp++;

		//TODO: check for timeouts in remote hosts
	}


	void LocalHost::receive() {
		m_unverified_count = 0;
		for(int n = 0; n < (int)m_remote_hosts.size(); n++) {
			RemoteHost *remote = m_remote_hosts[n].get();
			if(!remote)
				continue;

			if(!remote->isVerified())
				m_unverified_count++;
			remote->beginReceiving();
		}

		while(true) {
			InPacket packet;
			Address source;

			int new_size = m_socket.receive(packet.m_data, sizeof(packet.m_data), source);
			if(new_size == 0)
				break;

			if(new_size < PacketInfo::header_size)
				continue;

			packet.ready(new_size);
			if(packet.info().protocol_id != PacketInfo::valid_protocol_id)
				continue;

			int remote_id = packet.currentId();
			int current_id = packet.remoteId();

			if(current_id == -1 && m_unverified_count < max_unverified_hosts) {
				for(int r = 0; r < (int)m_remote_hosts.size(); r++) {
					if(m_remote_hosts[r] && m_remote_hosts[r]->address() == source) {
						current_id = r;
						break;
					}
				}

				//TODO: blacklist filtering?
				current_id = addRemoteHost(source, remote_id);
				m_unverified_count++;
			}

			if(current_id >= 0 && current_id < numRemoteHosts()) {
				RemoteHost *remote = m_remote_hosts[current_id].get();
				if(remote && remote->address() == source)
					m_remote_hosts[current_id]->receive(packet, m_timestamp);
			}
		}

		for(int n = 0; n < (int)m_remote_hosts.size(); n++) {
			RemoteHost *remote = m_remote_hosts[n].get();
			if(remote)
				remote->finishReceiving();
		}
	}

	int LocalHost::addRemoteHost(const Address &address, int remote_id) {
		int idx = -1;
		for(int n = 0; n < (int)m_remote_hosts.size(); n++) {
			if(m_remote_hosts[n]) {
				if(m_remote_hosts[n]->address() == address)
					return n;
			}
			else if(idx == -1)
				idx = n;
		}

		if(idx == -1) {
			if(m_remote_hosts.size() == max_remote_hosts)
				return -1;
			m_remote_hosts.emplace_back();
			idx = (int)m_remote_hosts.size() - 1;
		}

		m_remote_hosts[idx] =
			std::move(unique_ptr<RemoteHost>(new RemoteHost(address, PacketInfo::max_size * 4, idx, remote_id)));
		
		return idx;
	}

	const RemoteHost *LocalHost::getRemoteHost(int id) const {
		DASSERT(id >= 0 && id < numRemoteHosts());
		return m_remote_hosts[id].get();
	}
	
	RemoteHost *LocalHost::getRemoteHost(int id) {
		DASSERT(id >= 0 && id < numRemoteHosts());
		return m_remote_hosts[id].get();
	}

#undef INSERT
#undef REMOVE
		
	void LocalHost::removeRemoteHost(int remote_id) {
		DASSERT(remote_id >= 0 && remote_id < numRemoteHosts());
		delete m_remote_hosts[remote_id].release();
	}
		
	void LocalHost::beginSending(int remote_id) {
		DASSERT(m_current_id == -1);
		DASSERT(remote_id >= 0 && remote_id < (int)m_remote_hosts.size());
		DASSERT(m_remote_hosts[remote_id]);

		m_current_id = remote_id;
		m_remote_hosts[remote_id]->beginSending(&m_socket);
	}
		
		
	void LocalHost::enqueChunk(const char *data, int data_size, ChunkType type, int channel_id) {
		DASSERT(m_current_id != -1);
		m_remote_hosts[m_current_id]->enqueChunk(data, data_size, type, channel_id);
	}

	bool LocalHost::enqueUChunk(const char *data, int data_size, ChunkType type, int identifier, int channel_id) {
		DASSERT(m_current_id != -1);
		return m_remote_hosts[m_current_id]->enqueUChunk(data, data_size, type, identifier, channel_id);
	}

	void LocalHost::finishSending() {
		DASSERT(m_current_id != -1);
		m_remote_hosts[m_current_id]->finishSending();
		m_current_id = -1;
	}

/*	bool LocalHost::receiveChunk(PodArray<char> &data, ChunkInfo &info) {

	}

	int LocalHost::getLostChunks(int target_id, int *buffer, int buffer_size) {

	}*/

}