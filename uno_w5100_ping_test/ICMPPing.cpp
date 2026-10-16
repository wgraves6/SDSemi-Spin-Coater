/*
 * Copyright (c) 2010 by Blake Foster <blfoster@vassar.edu>
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2
 * or the GNU Lesser General Public License version 2.1, both as
 * published by the Free Software Foundation.
 */

#include "ICMPPing.h"

#ifdef ICMPPING_INSERT_YIELDS
#define ICMPPING_DOYIELD()		delay(2)
#else
#define ICMPPING_DOYIELD()
#endif


// Holds the Ethernet SPI settings for the lifetime of a scope. Ethernet 2.x's
// W5100.read()/write() don't take the SPI transaction themselves (the library's
// socket layer does), and the SD card shares this bus with different settings.
struct EthernetSpiGuard
{
    EthernetSpiGuard()  { SPI.beginTransaction(SPI_ETHERNET_SETTINGS); }
    ~EthernetSpiGuard() { SPI.endTransaction(); }
};

// Replacements for the TX/RX buffer helpers Ethernet 2.x made private
// (mirrors write_data()/read_data()/getSnRX_RSR() in its socket.cpp).
static void writeTxBuffer(uint8_t s, const uint8_t * data, uint16_t len)
{
    uint16_t ptr = W5100.readSnTX_WR(s);
    uint16_t offset = ptr & W5100.SMASK;
    uint16_t dstAddr = offset + W5100.SBASE(s);

    if (W5100.hasOffsetAddressMapping() || offset + len <= W5100.SSIZE) {
        W5100.write(dstAddr, data, len);
    } else {
        // wrap around the circular buffer (W5100 only; W5500 maps offsets itself)
        uint16_t size = W5100.SSIZE - offset;
        W5100.write(dstAddr, data, size);
        W5100.write(W5100.SBASE(s), data + size, len - size);
    }
    W5100.writeSnTX_WR(s, ptr + len);
}

static void readRxBuffer(uint8_t s, uint16_t src, uint8_t * dst, uint16_t len)
{
    uint16_t offset = src & W5100.SMASK;
    uint16_t srcAddr = W5100.RBASE(s) + offset;

    if (W5100.hasOffsetAddressMapping() || offset + len <= W5100.SSIZE) {
        W5100.read(srcAddr, dst, len);
    } else {
        uint16_t size = W5100.SSIZE - offset;
        W5100.read(srcAddr, dst, size);
        W5100.read(W5100.RBASE(s), dst + size, len - size);
    }
}

static uint16_t rxReceivedSize(uint8_t s)
{
    // The register can change mid-read while a packet arrives; read until stable.
    uint16_t prev = W5100.readSnRX_RSR(s);
    for (;;) {
        uint16_t val = W5100.readSnRX_RSR(s);
        if (val == prev) return val;
        prev = val;
    }
}

// Big-endian field access by bytes - avoids unaligned/aliased pointer casts.
static inline void put16(uint8_t * p, uint16_t v) { p[0] = v >> 8; p[1] = v; }
static inline void put32(uint8_t * p, uint32_t v) { put16(p, v >> 16); put16(p + 2, v); }
static inline uint16_t get16(const uint8_t * p) { return ((uint16_t)p[0] << 8) | p[1]; }
static inline uint32_t get32(const uint8_t * p) { return ((uint32_t)get16(p) << 16) | get16(p + 2); }

inline uint16_t _makeUint16(const uint8_t& highOrder, const uint8_t& lowOrder)
{
    // make a 16-bit unsigned integer given the low order and high order bytes.
    // lowOrder first because the Arduino is little endian.
    uint8_t value [] = {lowOrder, highOrder};
    return *(uint16_t *)&value;
}

uint16_t _checksum(const ICMPEcho& echo)
{
    // calculate the checksum of an ICMPEcho with all fields but icmpHeader.checksum populated
    unsigned long sum = 0;

    // add the header, bytes reversed since we're using little-endian arithmetic.
    sum += _makeUint16(echo.icmpHeader.type, echo.icmpHeader.code);

    // add id and sequence
    sum += echo.id + echo.seq;

    // add time, one half at a time.
    uint16_t const * time = (uint16_t const *)&echo.time;
    sum += *time + *(time + 1);
    
    // add the payload
    for (uint8_t const * b = echo.payload; b < echo.payload + sizeof(echo.payload); b+=2)
    {
        sum += _makeUint16(*b, *(b + 1));
    }

    // ones complement of ones complement sum
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return ~sum;
}

ICMPEcho::ICMPEcho(uint8_t type, uint16_t _id, uint16_t _seq, uint8_t * _payload)
: id(_id), seq(_seq), time(millis())
{
    memcpy(payload, _payload, REQ_DATASIZE);
    icmpHeader.type = type;
    icmpHeader.code = 0;
    icmpHeader.checksum = _checksum(*this);
}

ICMPEcho::ICMPEcho()
: id(0), seq(0), time(0)
{
    memset(payload, 0, sizeof(payload));
    icmpHeader.code = 0;
    icmpHeader.type = 0;
    icmpHeader.checksum = 0;
}

void ICMPEcho::serialize(uint8_t * binData) const
{
    *(binData++) = icmpHeader.type;
    *(binData++) = icmpHeader.code;

    put16(binData, icmpHeader.checksum); binData += 2;
    put16(binData, id);                  binData += 2;
    put16(binData, seq);                 binData += 2;
    put32(binData, time);                binData += 4;

    memcpy(binData, payload, sizeof(payload));
}

void ICMPEcho::deserialize(uint8_t const * binData)
{
    icmpHeader.type = *(binData++);
    icmpHeader.code = *(binData++);

    icmpHeader.checksum = get16(binData); binData += 2;
    id                  = get16(binData); binData += 2;
    seq                 = get16(binData); binData += 2;

    if (icmpHeader.type != TIME_EXCEEDED)
    {
        time = get32(binData); binData += 4;
    }

    memcpy(payload, binData, sizeof(payload));
}


uint16_t ICMPPing::ping_timeout = PING_TIMEOUT;

ICMPPing::ICMPPing(uint8_t socket, uint8_t id) :
#ifdef ICMPPING_ASYNCH_ENABLE
  _curSeq(0), _numRetries(0), _asyncstart(0), _asyncstatus(BAD_RESPONSE),
#endif
  _id(id), _nextSeq(0), _socket(socket),  _attempt(0)
{
    memset(_payload, 0x1A, REQ_DATASIZE);
}


void ICMPPing::setPayload(uint8_t * payload)
{
	memcpy(_payload, payload, REQ_DATASIZE);
}

void ICMPPing::openSocket()
{
    EthernetSpiGuard spi;
    W5100.execCmdSn(_socket, Sock_CLOSE);
    W5100.writeSnIR(_socket, 0xFF);
    W5100.writeSnMR(_socket, SnMR::IPRAW);
    W5100.writeSnPROTO(_socket, IPPROTO::ICMP);
    W5100.writeSnPORT(_socket, 0);
    W5100.execCmdSn(_socket, Sock_OPEN);
}



void ICMPPing::operator()(const IPAddress& addr, int nRetries, ICMPEchoReply& result)
{
	openSocket();

    ICMPEcho echoReq(ICMP_ECHOREQ, _id, _nextSeq++, _payload);

    for (_attempt=0; _attempt<nRetries; ++_attempt)
    {

    	ICMPPING_DOYIELD();

        result.status = sendEchoRequest(addr, echoReq);
        if (result.status == SUCCESS)
        {
        	ICMPPING_DOYIELD();
            receiveEchoReply(echoReq, addr, result);
        }
        if (result.status == SUCCESS)
        {
            break;
        }
    }

    EthernetSpiGuard spi;
    W5100.execCmdSn(_socket, Sock_CLOSE);
    W5100.writeSnIR(_socket, 0xFF);
}

ICMPEchoReply ICMPPing::operator()(const IPAddress& addr, int nRetries)
{
    ICMPEchoReply reply;
    operator()(addr, nRetries, reply);
    return reply;
}

Status ICMPPing::sendEchoRequest(const IPAddress& addr, const ICMPEcho& echoReq)
{
    // I wish there were a better way of doing this, but if we use the uint32_t
    // cast operator, we're forced to (1) cast away the constness, and (2) deal
    // with an endianness nightmare.
    uint8_t addri [] = {addr[0], addr[1], addr[2], addr[3]};
    EthernetSpiGuard spi;
    W5100.writeSnDIPR(_socket, addri);
    W5100.writeSnTTL(_socket, 128);
    // The port isn't used, becuause ICMP is a network-layer protocol. So we
    // write zero. This probably isn't actually necessary.
    W5100.writeSnDPORT(_socket, 0);

    uint8_t serialized [sizeof(ICMPEcho)];
    echoReq.serialize(serialized);

    writeTxBuffer(_socket, serialized, sizeof(ICMPEcho));
    W5100.execCmdSn(_socket, Sock_SEND);

    while ((W5100.readSnIR(_socket) & SnIR::SEND_OK) != SnIR::SEND_OK) 
    {
        if (W5100.readSnIR(_socket) & SnIR::TIMEOUT)
        {
            W5100.writeSnIR(_socket, (SnIR::SEND_OK | SnIR::TIMEOUT));
            return SEND_TIMEOUT;
        }

        ICMPPING_DOYIELD();
    }
    W5100.writeSnIR(_socket, SnIR::SEND_OK);
    return SUCCESS;
}

void ICMPPing::receiveEchoReply(const ICMPEcho& echoReq, const IPAddress& addr, ICMPEchoReply& echoReply)
{
    icmp_time_t start = millis();
    while (millis() - start < ping_timeout)
    {

        EthernetSpiGuard spi;
        if (rxReceivedSize(_socket) < 1)
        {
        	// take a break, maybe let platform do
        	// some background work (like on ESP8266)
        	ICMPPING_DOYIELD();
        	continue;
        }

        // ah! we did receive something... check it out.
        // IPRAW receive format: 4-byte source IP, 2-byte length, then the ICMP data.

        uint8_t ipHeader[6];
		uint16_t ptr = W5100.readSnRX_RD(_socket);
		readRxBuffer(_socket, ptr, ipHeader, sizeof(ipHeader));
		ptr += sizeof(ipHeader);
		for (int i = 0; i < 4; ++i)
			echoReply.addr[i] = ipHeader[i];
		uint16_t dataLen = ((uint16_t)ipHeader[4] << 8) | ipHeader[5];

		uint8_t serialized[sizeof(ICMPEcho)];
		memset(serialized, 0, sizeof(serialized));
		uint16_t copyLen = dataLen < sizeof(ICMPEcho) ? dataLen : sizeof(ICMPEcho);
		readRxBuffer(_socket, ptr, serialized, copyLen);
		echoReply.data.deserialize(serialized);

		// skip the whole packet, even the part we didn't copy, so the next read
		// starts on a packet boundary
		ptr += dataLen;
		W5100.writeSnRX_RD(_socket, ptr);
		W5100.execCmdSn(_socket, Sock_RECV);

		echoReply.ttl = W5100.readSnTTL(_socket);

		// Since there aren't any ports in ICMP, we need to manually inspect the response
		// to see if it originated from the request we sent out.
		switch (echoReply.data.icmpHeader.type) {
		case ICMP_ECHOREP: {
			if (echoReply.data.id == echoReq.id
					&& echoReply.data.seq == echoReq.seq) {
				echoReply.status = SUCCESS;
				return;
			}
			break;
		}
		case TIME_EXCEEDED: {
			uint8_t * sourceIpHeader = echoReply.data.payload;
			unsigned int ipHeaderSize = (sourceIpHeader[0] & 0x0F) * 4u;
			uint8_t * sourceIcmpHeader = echoReply.data.payload + ipHeaderSize;

			// The destination ip address in the originating packet's IP header.
			IPAddress sourceDestAddress(sourceIpHeader + ipHeaderSize - 4);

			if (!(sourceDestAddress == addr))
				continue;

			uint16_t sourceId = ntohs(*(uint16_t * )(sourceIcmpHeader + 4));
			uint16_t sourceSeq = ntohs(*(uint16_t * )(sourceIcmpHeader + 6));

			if (sourceId == echoReq.id && sourceSeq == echoReq.seq) {
				echoReply.status = BAD_RESPONSE;
				return;
			}
			break;
		}
		}


    }
    echoReply.status = NO_RESPONSE;
}



#ifdef ICMPPING_ASYNCH_ENABLE
/*
 * When ICMPPING_ASYNCH_ENABLE is defined, we have access to the
 * asyncStart()/asyncComplete() methods from the API.
 */
bool ICMPPing::asyncSend(ICMPEchoReply& result)
{
    ICMPEcho echoReq(ICMP_ECHOREQ, _id, _curSeq, _payload);

    Status sendOpResult(NO_RESPONSE);
    bool sendSuccess = false;
    for (uint8_t i=_attempt; i<_numRetries; ++i)
    {
    	_attempt++;

    	ICMPPING_DOYIELD();
    	sendOpResult = sendEchoRequest(_addr, echoReq);
    	if (sendOpResult == SUCCESS)
    	{
    		sendSuccess = true; // it worked
    		sendOpResult = ASYNC_SENT; // we're doing this async-style, force the status
    		_asyncstart = millis(); // not the start time, for timeouts
    		break; // break out of this loop, 'cause we're done.

    	}
    }
    _asyncstatus = sendOpResult; // keep track of this, in case the ICMPEchoReply isn't re-used
    result.status = _asyncstatus; // set the result, in case the ICMPEchoReply is checked
    return sendSuccess; // return success of send op
}
bool ICMPPing::asyncStart(const IPAddress& addr, int nRetries, ICMPEchoReply& result)
{
	openSocket();

	// stash our state, so we can access
	// in asynchSend()/asyncComplete()
	_numRetries = nRetries;
	_attempt = 0;
	_curSeq = _nextSeq++;
	_addr = addr;

	return asyncSend(result);

}

bool ICMPPing::asyncComplete(ICMPEchoReply& result)
{

	if (_asyncstatus != ASYNC_SENT)
	{
		// we either:
		//  - didn't start an async request;
		//	- failed to send; or
		//	- are no longer waiting on this packet.
		// either way, we're done
		return true;
	}


	uint16_t pending;
	{
		EthernetSpiGuard spi;
		pending = rxReceivedSize(_socket);
	}
	if (pending)
	{
		// ooooh, we've got a pending reply
	    ICMPEcho echoReq(ICMP_ECHOREQ, _id, _curSeq, _payload);
		receiveEchoReply(echoReq, _addr, result);
		_asyncstatus = result.status; // make note of this status, whatever it is.
		return true; // whatever the result of the receiveEchoReply(), the async op is done.
	}

	// nothing yet... check if we've timed out
	if ( (millis() - _asyncstart) > ping_timeout)
	{

		// yep, we've timed out...
		if (_attempt < _numRetries)
		{
			// still, this wasn't our last attempt, let's try again
			if (asyncSend(result))
			{
				// another send has succeeded
				// we'll wait for that now...
				return false;
			}

			// this send has failed. too bad,
			// we are done.
			return true;
		}

		// we timed out and have no more attempts left...
		// hello?  is anybody out there?
		// guess not:
	    result.status = NO_RESPONSE;
	    return true;
	}

	// have yet to time out, will wait some more:
	return false; // results still not in

}

#endif	/* ICMPPING_ASYNCH_ENABLE */


