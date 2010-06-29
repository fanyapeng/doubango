/*
* Copyright (C) 2009 Mamadou Diop.
*
* Contact: Mamadou Diop <diopmamadou(at)doubango.org>
*	
* This file is part of Open Source Doubango Framework.
*
* DOUBANGO is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*	
* DOUBANGO is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*	
* You should have received a copy of the GNU General Public License
* along with DOUBANGO.
*
*/

/**@file tdav_consumer_audio.c
 * @brief Base class for all Audio consumers.
 *
 * @author Mamadou Diop <diopmamadou(at)doubango.org>
 *
 * @date Created: Sat Nov 8 16:54:58 2009 mdiop
 */
#include "tinydav/audio/tdav_consumer_audio.h"

#include "tinyrtp/rtp/trtp_rtp_header.h"

#include "tsk_memory.h"
#include "tsk_time.h"
#include "tsk_debug.h"

#if TSK_UNDER_WINDOWS
#	include <Winsock2.h> // timeval
#elif defined(__SYMBIAN32__)
#	include <_timeval.h> 
#else
#	include <sys/time.h>
#endif

#define TDAV_BITS_PER_SAMPLE_DEFAULT	16
#define TDAV_CHANNELS_DEFAULT			2
#define TDAV_RATE_DEFAULT				8000
#define TDAV_PTIME_DEFAULT				20

/** Initialize audio consumer */
int tdav_consumer_audio_init(tdav_consumer_audio_t* self)
{
	int ret;

	if(!self){
		TSK_DEBUG_ERROR("Invalid parameter");
		return -1;
	}
	/* base */
	if((ret = tmedia_consumer_init(TMEDIA_CONSUMER(self)))){
		return ret;
	}

	/* self (should be update by prepare() by using the codec's info)*/
	self->bits_per_sample = TDAV_BITS_PER_SAMPLE_DEFAULT;
	self->channels = TDAV_CHANNELS_DEFAULT;
	self->rate = TDAV_RATE_DEFAULT;
	self->ptime = TDAV_PTIME_DEFAULT;

	/* self:jitterbuffer */
	if(!self->jb.jbuffer){
		self->jb.jbuffer = jb_new();
		self->jb.jcodec = JB_CODEC_OTHER; // FIXME: e.g. JB_CODEC_G711x
	}

	tsk_safeobj_init(self);

	return 0;
}

/**
* Generic function to compare two consumers.
* @param consumer1 The first consumer to compare.
* @param consumer2 The second consumer to compare.
* @retval Returns an integral value indicating the relationship between the two consumers:
* <0 : @a consumer1 less than @a consumer2.<br>
* 0  : @a consumer1 identical to @a consumer2.<br>
* >0 : @a consumer1 greater than @a consumer2.<br>
*/
int tdav_consumer_audio_cmp(const tsk_object_t* consumer1, const tsk_object_t* consumer2)
{	
	return (TDAV_CONSUMER_AUDIO(consumer1) - TDAV_CONSUMER_AUDIO(consumer2));
}

/* put data into the jitter buffer */
int tdav_consumer_audio_put(tdav_consumer_audio_t* self, void** data, const tsk_object_t* proto_hdr)
{
	const trtp_rtp_header_t* rtp_hdr = (const trtp_rtp_header_t*)proto_hdr;

	if(!self || !data || !*data || !self->jb.jbuffer || !rtp_hdr){
		TSK_DEBUG_ERROR("Invalid parameter");
		return -1;
	}
	/* synchronize the reference timestamp */
	if(!self->jb.ref_timestamp){
		struct timeval tv;
		long ts = (rtp_hdr->timestamp/(self->rate/1000));
		tsk_gettimeofday(&tv, tsk_null);
		tv.tv_sec -= (ts / self->rate);
		tv.tv_usec -= (ts % self->rate) * 125;
		if((tv.tv_usec -= (tv.tv_usec % (self->ptime * 10000))) <0){
			tv.tv_usec += 1000000;
			tv.tv_sec -= 1;
		}
		self->jb.ref_timestamp = tsk_time_get_ms(&tv);
	}
	
	tsk_safeobj_lock(self);
	jb_put(self->jb.jbuffer, *data, JB_TYPE_VOICE, self->ptime, (rtp_hdr->timestamp/(self->rate/1000)), (long) (tsk_time_now()-self->jb.ref_timestamp), self->jb.jcodec);
	*data = tsk_null;
	tsk_safeobj_unlock(self);

	return 0;
}

/* get data drom the jitter buffer */
void* tdav_consumer_audio_get(tdav_consumer_audio_t* self)
{
	void* data = tsk_null;
	int jret;

	if(!self || !self->jb.jbuffer){
		TSK_DEBUG_ERROR("Invalid parameter");
		return tsk_null;
	}	

	tsk_safeobj_lock(self);
	jret = jb_get(self->jb.jbuffer, (void**)&data, (long) (tsk_time_now()-self->jb.ref_timestamp), self->ptime);
	tsk_safeobj_unlock(self);

	switch(jret){
		case JB_OK:
			break;
		case JB_INTERP:
				jb_reset_all(self->jb.jbuffer);
			break;
		case JB_EMPTY:
		case JB_NOFRAME:
		case JB_NOJB:
		default:
			break;
	}

	return data;
}

/* tsk_safeobj_lock(self); */
/* tsk_safeobj_unlock(self); */

/** DeInitialize audio consumer */
int tdav_consumer_audio_deinit(tdav_consumer_audio_t* self)
{
	int ret;

	if(!self){
		TSK_DEBUG_ERROR("Invalid parameter");
		return -1;
	}

	/* base */
	if((ret = tmedia_consumer_deinit(TMEDIA_CONSUMER(self)))){
		/* return ret; */
	}

	/* self */
	if(self->jb.jbuffer){
		jb_destroy(self->jb.jbuffer);
	}

	tsk_safeobj_deinit(self);

	return 0;
}
