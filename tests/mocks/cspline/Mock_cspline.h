#ifndef __MOCK_CSPLINE_H__
#define __MOCK_CSPLINE_H__

#ifdef TEST
cspline_t cspline_alloc(uint32_t size);
cspline_t cspline_static_alloc(uint32_t size, float** membank);
cspline_mempool_t cspline_alloc_mempool(uint32_t size);
cspline_mempool_t cspline_static_alloc_mempool(float** membank);
void cspline_init(cspline_t* cspline, cspline_mempool_t mempool, float* x, float* y);
void cspline_update_size(cspline_t* cspline, uint32_t size);
float cspline_get_interpolated_point(cspline_t* cspline, float x);
void cspline_free(cspline_t cspline);
void cspline_free_mempool(cspline_mempool_t mempool);
#endif//TEST

#endif//__MOCK_CSPLINE_H__