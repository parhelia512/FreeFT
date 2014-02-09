/* Copyright (C) 2013-2014 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of FreeFT.
 */

#ifndef SYS_HOST_H
#define SYS_HOST_H

#include "sys/network.h"

namespace net {

	struct Chunk {
		Chunk() { }
		Chunk(const char *data, int data_size, ChunkType type, int chunk_id, int channel_id);

		PodArray<char> data;

		// It can be on one of 3 lists:
		// packet list:  waiting for an ack with specified packet_id
		// channel list: waiting to be sent
		// free list: unused chunk
		ListNode node;
		int chunk_id;
		SeqNumber packet_id;
		ChunkType type;
		u8 channel_id;

		//TODO: inline data (fill up to 64 bytes)
	};

	class InChunk :public Stream {
	public:
		InChunk(const Chunk *immutable_chunk);
		operator bool() const { return m_chunk != nullptr; }

		bool end() const { return m_pos == m_size; }

		int size() const { return m_size; }
		int chunkId() const { return m_chunk? m_chunk->chunk_id : 0; }
		int channelId() const { return m_chunk? m_chunk->channel_id : 0; }
		ChunkType type() const { return m_chunk? m_chunk->type : ChunkType::invalid; }

	protected:
		virtual void v_load(void *ptr, int count) final;

		const Chunk *m_chunk;
	};

	struct UChunk {
		int chunk_id;
		int channel_id;
		ListNode node;
		SeqNumber packet_id;
	};

	class RemoteHost {
	public:
		RemoteHost(const Address &address, int max_bytes_per_frame, int current_id, int remote_id);

		enum {
			max_channels = 8,
			max_unacked_packets = 16, //TODO: max ack time would be better
			max_ack_per_frame = max_unacked_packets * 2,
		};

		struct Packet {
			Packet() :packet_id(0) { }

			SeqNumber packet_id;
			List chunks, uchunks;
		};

		struct Channel {
			Channel() :data_size(0), last_chunk_id(0), last_ichunk_id(0) { }

			List chunks;
			int data_size;
			int last_chunk_id;
			int last_ichunk_id;
		};

		void beginSending(Socket *socket);
		void  enqueChunk(const TempPacket &data, ChunkType type, int channel_id);
		void  enqueChunk(const char *data, int data_size, ChunkType type, int channel_id);

		// Have to be in sending mode, to enque UChunk
		// TODO: better name, we're not really enquing anything
		bool enqueUChunk(const TempPacket &data, ChunkType type, int identifier, int channel_id);
		bool enqueUChunk(const char *data, int data_size, ChunkType type, int identifier, int channel_id);
		void finishSending();

		void beginReceiving();
		void receive(InPacket &packet, int timestamp);
		void handlePacket(InPacket& packet);
		void finishReceiving();
		const Chunk *getIChunk();

		const Address address() const { return m_address; }
		bool isVerified() const { return m_is_verified; }
		void verify(bool value) { m_is_verified = value; }

		vector<int> &lostUChunks() { return m_lost_uchunk_indices; }
		int lastTimestamp() const { return m_last_timestamp; }

	protected:
		void sendChunks(int max_channel);

		bool sendChunk(int chunk_idx);
		bool sendUChunk(int chunk_idx, const char *data, int data_size, ChunkType type);
		bool sendChunk(const char *data, int data_size, ChunkType type, int chunk_id, int channel_id, bool is_reliable);

		int estimateSize(int data_size) const;
		bool canFit(int data_size) const;
		bool isSending() const { return m_socket != nullptr; }

		int allocChunk() { return freeListAlloc<Chunk, &Chunk::node>(m_chunks, m_free_chunks); }
		int allocUChunk() { return freeListAlloc<UChunk, &UChunk::node>(m_uchunks, m_free_uchunks); }

		void freeChunk(int idx) { listInsert<Chunk, &Chunk::node>(m_chunks, m_free_chunks, idx); }
		void freeUChunk(int idx) { listInsert<UChunk, &UChunk::node>(m_uchunks, m_free_uchunks, idx); }

		void newPacket(bool is_first);
		void sendPacket();

		void resendPacket(int packet_idx);
		void acceptPacket(int packet_idx);

		Address m_address;
		vector<Chunk> m_chunks;
		vector<UChunk> m_uchunks;
		vector<Channel> m_channels;
		vector<int> m_ichunk_indices;
		vector<SeqNumber> m_out_acks, m_in_acks;

		LinkedVector<Packet> m_packets;
		LinkedVector<InPacket> m_in_packets;

		vector<int> m_lost_uchunk_indices;

		List m_free_chunks;
		List m_free_uchunks;
		List m_out_ichunks;

		int m_last_timestamp;
		int m_max_bpf;
		int m_current_id, m_remote_id;
		SeqNumber m_out_packet_id, m_in_packet_id;

		//TODO: special rules for un-verified hosts (limit packets per frame, etc.)
		bool m_is_verified;

		// Sending context
		Socket *m_socket;
		OutPacket m_out_packet;
		int m_packet_idx;
		int m_bytes_left;

		// Receiving context
		vector<int> m_current_ichunk_indices;
	};

	//TODO: network data verification (so that the game won't crash under any circumstances)
	class LocalHost {
	public:
		enum {
			max_remote_hosts = 32,
			max_unverified_hosts = 4,
		};

		LocalHost(const net::Address &address);

		void beginFrame();
		void finishFrame();

		void receive();

		// Returns -1 if cannot add more hosts
		int addRemoteHost(const Address &address, int remote_id);
		const RemoteHost *getRemoteHost(int id) const;
		RemoteHost *getRemoteHost(int id);
		int numRemoteHosts() const { return (int)m_remote_hosts.size(); }
		void removeRemoteHost(int remote_id);

		void beginSending(int remote_id);

		// Reliable chunks will get queued
		// Unreliable will be sent immediately or discarded (false will be returned)
		// Channels with lower id have higher priority;
		// If you are sending both reliable and unreliable packets in the same frame,
		// then you should add reliable packets first, so that they will be sent first
		// if they have higher priority
		void  enqueChunk(const char *data, int data_size, ChunkType type, int channel_id);
		bool enqueUChunk(const char *data, int data_size, ChunkType type, int identifier, int channel_id);
		
		void finishSending();
		int timestamp() const { return m_timestamp; }


	protected:
		net::Socket m_socket;
		vector<unique_ptr<RemoteHost>> m_remote_hosts;
		int m_unverified_count, m_remote_count;

		int m_current_id;
		int m_timestamp;
	};

}

#endif