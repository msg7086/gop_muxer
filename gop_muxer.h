extern "C" {
    #include <lsmash.h>
}
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <vector>
#include <queue>

#define SWAP(num) (((num>>24)&0xff) | ((num<<8)&0xff0000) | ((num>>8)&0xff00) | ((num<<24)&0xff000000))


lsmash_root_t *p_root;
lsmash_video_summary_t *summary;
lsmash_file_parameters_t file_param;

uint32_t i_numframe;

int i_delay_frames;
uint32_t i_movie_timescale;
uint32_t i_video_timescale;
uint32_t i_track;
uint32_t i_sample_entry;

uint32_t i_sei_size = 0;
uint8_t *p_sei_buffer = NULL;
int64_t i_start_offset;
uint64_t i_first_cts;

uint32_t input_timebase_num = 1001;

auto pts_queue = new std::priority_queue<int64_t>();

#define MP4_FAIL_IF_ERR MP4_LOG_IF_ERR

#define MP4_LOG_IF_ERR( cond, ... )\
if( cond )\
{\
    printf( __VA_ARGS__ );\
}
