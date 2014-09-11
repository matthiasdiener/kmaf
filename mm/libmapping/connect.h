#ifndef __LIBMAPPING_CONNECT_H__
#define __LIBMAPPING_CONNECT_H__

#ifndef _SPCD
	void libmapping_panic(int code);
#else
	#define libmapping_panic(var)
#endif

thread_t* libmapping_get_current_thread (void);
uint32_t libmapping_is_dynamic_mode (void);
uint32_t libmapping_initialized (void);

#endif
