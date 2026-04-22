/* ===== Channel Runtime (Windows native) =====
 * Same behavior as channel_posix.c using CRITICAL_SECTION + CONDITION_VARIABLE.
 */

#ifndef _WIN32
#error "channel_win32.c is for Windows builds only"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <windows.h>

#define RC_TYPE_CHANNEL 20

extern void* rc_alloc(int64_t type_tag, size_t payload_bytes);
extern void yona_rt_rc_inc(void* p);
extern void yona_rt_rc_dec(void* p);
extern void yona_rt_raise(int64_t symbol, const char* message);

struct yona_task_group;
typedef struct yona_task_group yona_task_group_t;
extern int yona_rt_group_is_cancelled(yona_task_group_t* g);

#define SYM_CANCELLED 0
#define SYM_DEADLOCK 0
#define SYM_CHANNEL_CLOSED 0

typedef struct yona_channel {
	int64_t cap;
	int64_t count;
	int64_t head;
	int64_t tail;
	int64_t* buf;
	CRITICAL_SECTION mutex;
	CONDITION_VARIABLE not_full;
	CONDITION_VARIABLE not_empty;
	int closed;
	int waiters;
	yona_task_group_t* group;
} yona_channel_t;

static int64_t* chan_make_some(int64_t value) {
	int64_t* adt = (int64_t*)rc_alloc(4, 4 * sizeof(int64_t));
	adt[0] = 0;
	adt[1] = 1;
	adt[2] = 0;
	adt[3] = value;
	return adt;
}

static int64_t* chan_make_none(void) {
	int64_t* adt = (int64_t*)rc_alloc(4, 3 * sizeof(int64_t));
	adt[0] = 1;
	adt[1] = 0;
	adt[2] = 0;
	return adt;
}

yona_channel_t* yona_rt_channel_new(int64_t cap) {
	if (cap < 1) cap = 1;
	yona_channel_t* ch = (yona_channel_t*)rc_alloc(RC_TYPE_CHANNEL, sizeof(yona_channel_t));
	ch->cap = cap;
	ch->count = 0;
	ch->head = 0;
	ch->tail = 0;
	ch->buf = (int64_t*)calloc((size_t)cap, sizeof(int64_t));
	InitializeCriticalSection(&ch->mutex);
	InitializeConditionVariable(&ch->not_full);
	InitializeConditionVariable(&ch->not_empty);
	ch->closed = 0;
	ch->waiters = 0;
	ch->group = NULL;
	return ch;
}

static int channel_should_timeout_check(int wait_count) {
	return wait_count > 50;
}

void yona_rt_channel_send(yona_channel_t* ch, int64_t value) {
	EnterCriticalSection(&ch->mutex);
	int wait_count = 0;
	while (ch->count == ch->cap && !ch->closed) {
		if (ch->group && yona_rt_group_is_cancelled(ch->group)) {
			LeaveCriticalSection(&ch->mutex);
			yona_rt_raise(SYM_CANCELLED, "task cancelled while waiting on channel send");
			return;
		}
		ch->waiters++;
		SleepConditionVariableCS(&ch->not_full, &ch->mutex, 100);
		ch->waiters--;
		wait_count++;
		if (channel_should_timeout_check(wait_count)) {
			LeaveCriticalSection(&ch->mutex);
			yona_rt_raise(SYM_DEADLOCK, "channel send blocked >5s — possible deadlock");
			return;
		}
	}
	if (ch->closed) {
		LeaveCriticalSection(&ch->mutex);
		yona_rt_raise(SYM_CHANNEL_CLOSED, "send on closed channel");
		return;
	}
	ch->buf[ch->tail] = value;
	ch->tail = (ch->tail + 1) % ch->cap;
	ch->count++;
	WakeConditionVariable(&ch->not_empty);
	LeaveCriticalSection(&ch->mutex);
}

int64_t yona_rt_channel_recv(yona_channel_t* ch) {
	EnterCriticalSection(&ch->mutex);
	int wait_count = 0;
	while (ch->count == 0 && !ch->closed) {
		if (ch->group && yona_rt_group_is_cancelled(ch->group)) {
			LeaveCriticalSection(&ch->mutex);
			yona_rt_raise(SYM_CANCELLED, "task cancelled while waiting on channel recv");
			return 0;
		}
		ch->waiters++;
		SleepConditionVariableCS(&ch->not_empty, &ch->mutex, 100);
		ch->waiters--;
		wait_count++;
		if (channel_should_timeout_check(wait_count)) {
			LeaveCriticalSection(&ch->mutex);
			yona_rt_raise(SYM_DEADLOCK, "channel recv blocked >5s — possible deadlock");
			return 0;
		}
	}
	if (ch->count == 0 && ch->closed) {
		LeaveCriticalSection(&ch->mutex);
		return (int64_t)(intptr_t)chan_make_none();
	}
	int64_t value = ch->buf[ch->head];
	ch->head = (ch->head + 1) % ch->cap;
	ch->count--;
	WakeConditionVariable(&ch->not_full);
	LeaveCriticalSection(&ch->mutex);
	return (int64_t)(intptr_t)chan_make_some(value);
}

int64_t yona_rt_channel_try_recv(yona_channel_t* ch) {
	EnterCriticalSection(&ch->mutex);
	if (ch->count == 0) {
		LeaveCriticalSection(&ch->mutex);
		return (int64_t)(intptr_t)chan_make_none();
	}
	int64_t value = ch->buf[ch->head];
	ch->head = (ch->head + 1) % ch->cap;
	ch->count--;
	WakeConditionVariable(&ch->not_full);
	LeaveCriticalSection(&ch->mutex);
	return (int64_t)(intptr_t)chan_make_some(value);
}

void yona_rt_channel_close(yona_channel_t* ch) {
	EnterCriticalSection(&ch->mutex);
	ch->closed = 1;
	WakeAllConditionVariable(&ch->not_full);
	WakeAllConditionVariable(&ch->not_empty);
	LeaveCriticalSection(&ch->mutex);
}

int64_t yona_rt_channel_is_closed(yona_channel_t* ch) {
	EnterCriticalSection(&ch->mutex);
	int closed = ch->closed;
	LeaveCriticalSection(&ch->mutex);
	return closed ? 1 : 0;
}

int64_t yona_rt_channel_length(yona_channel_t* ch) {
	EnterCriticalSection(&ch->mutex);
	int64_t n = ch->count;
	LeaveCriticalSection(&ch->mutex);
	return n;
}

int64_t yona_rt_channel_capacity(yona_channel_t* ch) {
	return ch->cap;
}

void yona_rt_channel_destroy(void* ptr) {
	yona_channel_t* ch = (yona_channel_t*)ptr;
	EnterCriticalSection(&ch->mutex);
	WakeAllConditionVariable(&ch->not_full);
	WakeAllConditionVariable(&ch->not_empty);
	LeaveCriticalSection(&ch->mutex);
	DeleteCriticalSection(&ch->mutex);
	free(ch->buf);
}

int64_t yona_Std_Channel__channel(int64_t cap) {
	return (int64_t)(intptr_t)yona_rt_channel_new(cap);
}

int64_t yona_Std_Channel__send(int64_t ch_i64, int64_t value) {
	yona_rt_channel_send((yona_channel_t*)(intptr_t)ch_i64, value);
	return 0;
}

int64_t yona_Std_Channel__recv(int64_t ch_i64) {
	return yona_rt_channel_recv((yona_channel_t*)(intptr_t)ch_i64);
}

int64_t yona_Std_Channel__tryRecv(int64_t ch_i64) {
	return yona_rt_channel_try_recv((yona_channel_t*)(intptr_t)ch_i64);
}

int64_t yona_Std_Channel__close(int64_t ch_i64) {
	yona_rt_channel_close((yona_channel_t*)(intptr_t)ch_i64);
	return 0;
}

int64_t yona_Std_Channel__isClosed(int64_t ch_i64) {
	return yona_rt_channel_is_closed((yona_channel_t*)(intptr_t)ch_i64);
}

int64_t yona_Std_Channel__length(int64_t ch_i64) {
	return yona_rt_channel_length((yona_channel_t*)(intptr_t)ch_i64);
}

int64_t yona_Std_Channel__capacity(int64_t ch_i64) {
	return yona_rt_channel_capacity((yona_channel_t*)(intptr_t)ch_i64);
}

struct yona_promise;
typedef struct yona_promise yona_promise_t;
extern yona_promise_t* yona_rt_async_spawn_closure(int64_t* closure, yona_task_group_t* group);

int64_t yona_Std_Task__spawn(int64_t* closure) {
	return (int64_t)(intptr_t)yona_rt_async_spawn_closure(closure, NULL);
}
