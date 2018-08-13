#ifndef QUEUEX_H
#define QUEUEX_H

struct QueueT
{
	uint8_t* Buffer;
	uint32_t BufferSize;
	
	uint32_t Count;
	uint32_t Head;
	uint32_t Tail;
};

void QueueInit(struct QueueT* q, uint8_t* buffer, const uint32_t bufferSize);
void QueueAppend(struct QueueT* q, const uint8_t x);
uint8_t QueueGet(struct QueueT* q);
uint8_t QueuePeek(struct QueueT* q, const uint32_t location);
uint8_t QueueAppendBuffer(struct QueueT* q, const uint8_t* x, const uint32_t appendSize);
uint8_t QueueEmpty(const struct QueueT* q);
void QueueDequeqe(struct QueueT* q, const uint32_t n);
uint8_t QueuePeekBuffer(struct QueueT* q, const uint32_t location, uint8_t* buffer, const uint32_t len);
uint32_t QueueCount(const struct QueueT* q);

#endif // !QUEUE_H



