#include <stdint.h>
#include <string.h>
#include "Queuex.h"
#include "stdint.h"

void QueueInit(struct QueueT* q, uint8_t* buffer, const uint32_t bufferSize)
{
	q->Buffer = buffer;
	q->BufferSize = bufferSize;
	
	q->Head = 0;
	q->Tail = 0;
	q->Count = 0;
}

uint32_t QueueCount(const struct QueueT* q)
{
	return q->Count;
}

void QueueAppend(struct QueueT* q, const uint8_t x)
{
	// Append the data and make the new tail location
	q->Buffer[q->Tail] = x;
	q->Tail = (++q->Tail % q->BufferSize);
	
	// Check if we can expand the count any more
	if (q->Count < q->BufferSize)
	{
		q->Count++;
	}
}

uint8_t QueueEmpty(const struct QueueT* q)
{
	return (q->Count == 0);
}

uint8_t QueueGet(struct QueueT* q)
{
	uint8_t x = q->Buffer[q->Head];
	q->Head = (++q->Head % q->BufferSize);
	q->Count--;
	return x;
}

void QueueDequeqe(struct QueueT* q, const uint32_t n)
{
	if (n > q->Count)
	{
		return;
	}

	q->Count -= n;
	q->Head = (q->Head + n) % q->BufferSize;
}

uint8_t QueuePeek(struct QueueT* q, const uint32_t location)
{
	return q->Buffer[(q->Head + location) % q->BufferSize];
}

uint8_t QueueAppendBuffer(struct QueueT* q, const uint8_t* x, const uint32_t appendSize)
{
	uint32_t bufferEndSize = q->BufferSize - q->Tail;
	
	// Check if we can even put this buffer into the queu
	if (q->Count + appendSize > q->BufferSize)
	{
		return 0;
	}

	// Check if there is enough space on the end of the queue to put the entire buffer
	if(bufferEndSize >= appendSize)
	{
		// We can write the entire append buffer in one shot
		memcpy((void*)(q->Buffer + q->Tail), (void*)x, appendSize);
	}
	else
	{
		memcpy((void*)(q->Buffer + q->Tail), (void*)x, bufferEndSize);
		memcpy((void*)(q->Buffer), (void*)(x + bufferEndSize), appendSize - bufferEndSize);
	}
	
	// Append to the tail
	q->Tail = ((q->Tail + appendSize) % q->BufferSize);
	q->Count += appendSize;
	
	return 1;
}

uint8_t QueuePeekBuffer(struct QueueT* q, const uint32_t location, uint8_t* buffer, const uint32_t len)
{
	uint32_t copyStart = q->Head + location;

	// Check to see if this amount of length exists at the location
	if (location + len > q->Count)
	{
		return 0;
	}

	// Check if we can do this in a single shot
	if (copyStart + len <= q->BufferSize)
	{
		memcpy((void*)buffer, (void*)(q->Buffer + copyStart), len);
	}
	else
	{
		// Double shot copy
		uint32_t copy1Size = (q->BufferSize - copyStart);
		memcpy((void*)buffer, (void*)(q->Buffer + copyStart), copy1Size);
		memcpy((void*)(buffer + copy1Size), (void*)(q->Buffer), (len - copy1Size));
	}

	return 1;
}