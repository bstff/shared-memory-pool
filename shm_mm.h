#ifndef __SHM_MM_H__
#define __SHM_MM_H__

#ifdef __cplusplus
extern "C" {
#endif
	
/**
 * 初始化共享内存
 * @size 共享内存大小, 单位M
 * 
 */
void shm_init(const int key, int size);

/**
 * 销毁共享内存
 * 整个进程退出时需要执行这个方法，该方法首先会检查是否还有其他进程在使用该共享内存，如果还有其他进程在使用就只是detach,如果没有其他进程在使用则销毁整块内存。
 */
void shm_destroy();


#ifdef __cplusplus
}
#endif

#endif

