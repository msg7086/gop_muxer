#include "gop_muxer.h"

using namespace std;

vector<char *> data_list = {};
const char* dir_prefix;
char* name_prefix;
char out_filename[10240] = {0};
char opt_filename[10240] = {0};
char hdr_filename[10240] = {0};

uint32_t progress = 0;
uint32_t total = 0;
int fail = 0;

void show_progress(const char * item) {
    printf("%7d/%d %.2lf%% %s     \r", progress, total, progress * 100.0 / total, item);
}

void load_gop_file(const char * gop_filename) {
    auto fp = fopen(gop_filename, "r");
    char buffer[8192];
    while(!feof(fp)) {
        buffer[0] = 0;
        fgets(buffer, 8192, fp);
        auto len = strlen(buffer);
        if(buffer[len-1] == '\n') buffer[len-1] = 0;
        if(buffer[0] == 0) continue;
        if(strncmp(buffer, "#options", 8) == 0) {
            if(!opt_filename[0])
                sprintf(opt_filename, "%s%s", dir_prefix, buffer + 9);
        }
        else if(strncmp(buffer, "#headers", 8) == 0) {
            if(!hdr_filename[0])
                sprintf(hdr_filename, "%s%s", dir_prefix, buffer + 9);
        }
        else {
            auto data_filename = new char[strlen(dir_prefix) + strlen(buffer) + 1];
            sprintf(data_filename, "%s%s", dir_prefix, buffer);
            data_list.push_back(data_filename);
        }
    }
    fclose(fp);

    p_root = lsmash_create_root();
    lsmash_open_file(out_filename, 0, &file_param);
    summary = (lsmash_video_summary_t *)lsmash_create_summary(LSMASH_SUMMARY_TYPE_VIDEO);
    summary->sample_type = ISOM_CODEC_TYPE_HVC1_VIDEO;

    printf("%s loaded.\n", gop_filename);
}

void load_options_file(const char * opt_filename) {
    uint32_t b_frames           = 1;
    uint32_t b_pyramid          = 1;
    uint32_t input_timebase_den = 24000;
    uint32_t output_fps_num     = 24000;
    uint32_t output_fps_den     = 1001;
    uint32_t source_width       = 1920;
    uint32_t source_height      = 1080;
    uint32_t sar_width          = 0;
    uint32_t sar_height         = 0;
    uint32_t primaries_index    = 2;
    uint32_t transfer_index     = 2;
    uint32_t matrix_index       = 2;
    uint32_t full_range         = 0;

    auto fp = fopen(opt_filename, "r");
    char buffer[8192], keyword[8192];
    uint32_t value;
    while(!feof(fp)) {
        fgets(buffer, 8192, fp);
        auto len = strlen(buffer);
        if(buffer[len-1] == '\n') buffer[len-1] = 0;
        if(buffer[0] == 0) continue;
        if(2 != sscanf(buffer, "%s %u", keyword, &value)) continue;

        if(strcmp(keyword, "b-frames")                == 0) b_frames           = value;
        else if(strcmp(keyword, "b-pyramid")          == 0) b_pyramid          = value;
        else if(strcmp(keyword, "input-timebase-num") == 0) input_timebase_num = value;
        else if(strcmp(keyword, "input-timebase-den") == 0) input_timebase_den = value;
        else if(strcmp(keyword, "output-fps-num")     == 0) output_fps_num     = value;
        else if(strcmp(keyword, "output-fps-den")     == 0) output_fps_den     = value;
        else if(strcmp(keyword, "source-width")       == 0) source_width       = value;
        else if(strcmp(keyword, "source-height")      == 0) source_height      = value;
        else if(strcmp(keyword, "sar-width")          == 0) sar_width          = value;
        else if(strcmp(keyword, "sar-height")         == 0) sar_height         = value;
        else if(strcmp(keyword, "primaries-index")    == 0) primaries_index    = value;
        else if(strcmp(keyword, "transfer-index")     == 0) transfer_index     = value;
        else if(strcmp(keyword, "matrix-index")       == 0) matrix_index       = value;
        else if(strcmp(keyword, "full-range")         == 0) full_range         = value;
        else printf("Ignored unknown option: %s\n", buffer);
    }
    fclose(fp);

    i_numframe = 0;
    i_delay_frames = b_frames ? (b_pyramid ? 2 : 1) : 0;
    // ##### i_time_inc = (uint64_t)info.timebaseNum;

    /* Select brands. */
    lsmash_brand_type brands[6] = { static_cast<lsmash_brand_type>(0) };
    uint32_t brand_count = 0;
    brands[brand_count++] = ISOM_BRAND_TYPE_MP42;
    brands[brand_count++] = ISOM_BRAND_TYPE_MP41;
    brands[brand_count++] = ISOM_BRAND_TYPE_ISOM;

    /* Set file */
    lsmash_file_parameters_t *fparam = &file_param;
    fparam->major_brand   = brands[0];
    fparam->brands        = brands;
    fparam->brand_count   = brand_count;
    fparam->minor_version = 0;
    lsmash_set_file(p_root, fparam);

    /* Set movie parameters. */
    lsmash_movie_parameters_t movie_param;
    lsmash_initialize_movie_parameters(&movie_param);
    lsmash_set_movie_parameters(p_root, &movie_param);
    i_movie_timescale = lsmash_get_movie_timescale(p_root);

    /* Create a video track. */
    i_track = lsmash_create_track(p_root, ISOM_MEDIA_HANDLER_TYPE_VIDEO_TRACK);

    summary->width = source_width;
    summary->height = source_height;
    uint32_t i_display_width = source_width << 16;
    uint32_t i_display_height = source_height << 16;
    if(sar_width && sar_height)
    {
        double sar = (double)sar_width / sar_height;
        if(sar > 1.0)
            i_display_width *= sar;
        else
            i_display_height /= sar;
        summary->par_h = sar_width;
        summary->par_v = sar_height;
    }
    summary->color.primaries_index = primaries_index;
    summary->color.transfer_index  = transfer_index;
    summary->color.matrix_index    = matrix_index;
    summary->color.full_range      = full_range;

    /* Set video track parameters. */
    lsmash_track_parameters_t track_param;
    lsmash_initialize_track_parameters(&track_param);
    lsmash_track_mode track_mode = static_cast<lsmash_track_mode>(ISOM_TRACK_ENABLED | ISOM_TRACK_IN_MOVIE | ISOM_TRACK_IN_PREVIEW);
    track_param.mode = track_mode;
    track_param.display_width = i_display_width;
    track_param.display_height = i_display_height;
    lsmash_set_track_parameters(p_root, i_track, &track_param);

    /* Set video media parameters. */
    lsmash_media_parameters_t media_param;
    lsmash_initialize_media_parameters(&media_param);
    media_param.timescale = input_timebase_den;
    media_param.media_handler_name = strdup("L-SMASH Video Media Handler");
    lsmash_set_media_parameters(p_root, i_track, &media_param);
    i_video_timescale = lsmash_get_media_timescale(p_root, i_track);

    show_progress(opt_filename);
}

void load_headers_file(const char * hdr_filename) {
    auto fp = fopen(hdr_filename, "r");
    uint8_t* buffer[4];
    uint32_t size[4] = {0};
    int i = 0;
    while(!feof(fp)) {
        int sizebuffer = 0;
        fread(&sizebuffer, sizeof(uint32_t), 1, fp);
        size[i] = SWAP(sizebuffer);
        buffer[i] = (uint8_t*)malloc(size[i] + 4);
        memcpy(buffer[i], &sizebuffer, sizeof(uint32_t));
        fread(buffer[i] + 4, sizeof(uint8_t), size[i], fp);
        i++;
        if(i > 3)
            break;
    }
    fclose(fp);
    if(i < 3) {
        printf("Expect 3 headers, %d given.\n", i);
        fail = -4;
        return;
    }

    auto cs = lsmash_create_codec_specific_data(
        LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_VIDEO_HEVC,
        LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED
        );

    auto param = (lsmash_hevc_specific_parameters_t *)cs->data.structured;
    param->lengthSizeMinusOne = 3;

    lsmash_append_hevc_dcr_nalu(param, HEVC_DCR_NALU_TYPE_VPS, buffer[0] + 4, size[0]); free(buffer[0]);
    lsmash_append_hevc_dcr_nalu(param, HEVC_DCR_NALU_TYPE_SPS, buffer[1] + 4, size[1]); free(buffer[1]);
    lsmash_append_hevc_dcr_nalu(param, HEVC_DCR_NALU_TYPE_PPS, buffer[2] + 4, size[2]); free(buffer[2]);
    lsmash_add_codec_specific_data((lsmash_summary_t *)summary, cs);
    lsmash_destroy_codec_specific_data(cs);
    i_sample_entry = lsmash_add_sample_entry(p_root, i_track, summary);

    if(size[3] > 0)
    {
        i_sei_size = size[3] + 4;
        p_sei_buffer = buffer[3];
    }

    show_progress(hdr_filename);
}

void write_frame(bool keyframe, int64_t pts, int64_t dts, uint8_t* payload, uint32_t size) {
    pts -= dts - i_numframe + i_delay_frames;
    dts = i_numframe - i_delay_frames;
    // printf("Write Frame: DTS: %8lld  PTS: %8lld\n", dts, pts);

    if (pts_queue) {
        pts_queue->push(-pts);
        if (pts_queue->size() > 2)
            pts_queue->pop();
    }

    const int i_pts = pts;
    const int i_dts = dts - i_delay_frames;

    if(!i_numframe)
    {
        i_start_offset = i_dts * -1;
        i_first_cts = (i_start_offset * input_timebase_num);
    }

    lsmash_sample_t *p_sample = lsmash_create_sample(size);

    uint8_t* pp = p_sample->data;
    memcpy(pp, payload, size);

    p_sample->dts = (i_dts + i_start_offset) * input_timebase_num;
    p_sample->cts = (i_pts + i_start_offset) * input_timebase_num;
    p_sample->index = i_sample_entry;
    p_sample->prop.ra_flags = keyframe ? ISOM_SAMPLE_RANDOM_ACCESS_FLAG_SYNC : ISOM_SAMPLE_RANDOM_ACCESS_FLAG_NONE;

    lsmash_append_sample(p_root, i_track, p_sample);
    i_numframe++;
}

void load_data_file(const char * data_filename) {
    auto fp = fopen(data_filename, "rb");
    uint8_t* buffer = NULL;
    uint32_t size = 0, blocksize = 0;
    int i = 0;
    int64_t pts, dts;
    bool keyframe = true;

    while(!feof(fp)) {
        int sizebuffer = 0;
        fread(&sizebuffer, sizeof(uint32_t), 1, fp);
        blocksize = SWAP(sizebuffer);
        if(blocksize == 0)
            break;
        if(blocksize == 16) {
            // Finish a frame
            if(size > 0) {
                write_frame(keyframe, pts, dts, buffer, size);
                keyframe = false;
                free(buffer);
                buffer = NULL;
                size = 0;
            }
            fread(&pts, sizeof(int64_t), 1, fp);
            fread(&dts, sizeof(int64_t), 1, fp);
            continue;
        }
        if(p_sei_buffer)
        {
            buffer = p_sei_buffer;
            size = i_sei_size;
            p_sei_buffer = NULL;
            i_sei_size = 0;
        }
        buffer = (uint8_t*) realloc(buffer, size + blocksize + sizeof(uint32_t));
        memcpy(buffer + size, &sizebuffer, sizeof(uint32_t));
        fread(buffer + size + sizeof(uint32_t), sizeof(uint8_t), blocksize, fp);
        size += blocksize + sizeof(uint32_t);
    }
    fclose(fp);

    if(size > 0)
        write_frame(keyframe, pts, dts, buffer, size);
    free(buffer);

    show_progress(data_filename);
}

void sign() {
    /* Write a tag in a free space to indicate the output file is written by L-SMASH. */
    const char *string = "Multiplexed by L-SMASH";
    int   length = strlen(string);
    lsmash_box_type_t type = lsmash_form_iso_box_type(LSMASH_4CC('f', 'r', 'e', 'e'));
    lsmash_box_t *free_box = lsmash_create_box(type, (uint8_t *)string, length, LSMASH_BOX_PRECEDENCE_N);
    if(!free_box)
        return;
    if(lsmash_add_box_ex(lsmash_root_as_box(p_root), &free_box) < 0)
    {
        lsmash_destroy_box(free_box);
        return;
    }
    lsmash_write_top_level_box(free_box);
}

void clean_up() {
    int64_t second_largest_pts = 0;
    int64_t largest_pts = 0;

    if (pts_queue && pts_queue->size() >= 2)
    {
        second_largest_pts = -pts_queue->top();
        pts_queue->pop();
        largest_pts = -pts_queue->top();
        pts_queue->pop();
        delete pts_queue;
        pts_queue = NULL;
    }

    if(p_root)
    {
        double actual_duration = 0;
        if(i_track)
        {
            /* Flush the rest of samples and add the last sample_delta. */
            uint32_t last_delta = largest_pts - second_largest_pts;
            lsmash_flush_pooled_samples(p_root, i_track, ((last_delta ? last_delta : 1) * input_timebase_num));

            actual_duration = ((double)((largest_pts + last_delta) * input_timebase_num) / i_video_timescale) * i_movie_timescale;

            lsmash_edit_t edit;
            edit.duration   = actual_duration;
            edit.start_time = i_first_cts;
            edit.rate       = ISOM_EDIT_MODE_NORMAL;
            lsmash_create_explicit_timeline_map(p_root, i_track, edit);
            lsmash_modify_explicit_timeline_map(p_root, i_track, 1, edit);
        }
        lsmash_finish_movie(p_root, NULL);
    }

    sign();

    lsmash_cleanup_summary((lsmash_summary_t *)summary);
    lsmash_close_file(&file_param);
    lsmash_destroy_root(p_root);
    delete p_sei_buffer;
}

void help(const char * argv[]) {
    printf("%s <file1.gop> [<file2.gop> ... <fileN.gop>]\n", argv[0]);
    puts("");
    puts("Mux given files with its segments into an mp4 file.");
    puts("");
    puts("CAUTION! 'fileN.mp4' will be overwritten without confirmation!");
    puts("");
}

int main(int argc, const char * argv[]) {
    if (argc < 2) {
        help(argv);
        return -1;
    }

    for (int i = 1; i < argc; i++) {
        const char* gop_filename = argv[i];
        const char* extension = gop_filename + strlen(gop_filename) - 4;
        if (strcmp(extension, ".gop") != 0) {
            printf("Only .gop file is accepted, %s given.\n", gop_filename);
            return -2;
        }

        const char* slash = strrchr(gop_filename, '/');
        if(slash == NULL) slash = strrchr(gop_filename, '\\');
        if(slash == NULL) {
            dir_prefix = "";
            slash = gop_filename;
        }
        else {
            int dir_len =  slash - gop_filename + 2;
            char* tmp_dir_prefix = new char[dir_len];
            strncpy(tmp_dir_prefix, gop_filename, dir_len - 1);
            tmp_dir_prefix[dir_len - 1] = 0;
            dir_prefix = tmp_dir_prefix;
            slash++;
        }
        int len = strlen(slash) - 3;
        name_prefix = new char[len];
        strncpy(name_prefix, slash, len - 1);
        name_prefix[len - 1] = 0;

        sprintf(out_filename, "%s%s.mp4", dir_prefix, name_prefix);

        printf("Dir: %-20s  File: %-20s\n", dir_prefix, name_prefix);

        load_gop_file(argv[i]);
    }

    if(opt_filename[0] == 0) { puts("Options file missing."); return -3; }
    if(hdr_filename[0] == 0) { puts("Headers file missing."); return -3; }
    if(data_list.size() == 0) { puts("Data files missing."); return -3; }

    total = data_list.size();
    printf("Saving to %s\n", out_filename);

    load_options_file(opt_filename);
    load_headers_file(hdr_filename);

    for (auto data_filename : data_list) {
        progress++;
        load_data_file(data_filename);
    }

    clean_up();

    return 0;
}
