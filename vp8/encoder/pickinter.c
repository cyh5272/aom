/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <limits.h>
#include "vpx_ports/config.h"
#include "onyx_int.h"
#include "modecosts.h"
#include "encodeintra.h"
#include "entropymode.h"
#include "pickinter.h"
#include "findnearmv.h"
#include "encodemb.h"
#include "reconinter.h"
#include "reconintra.h"
#include "reconintra4x4.h"
#include "g_common.h"
#include "variance.h"
#include "mcomp.h"

#include "vpx_mem/vpx_mem.h"

#if CONFIG_RUNTIME_CPU_DETECT
#define IF_RTCD(x) (x)
#else
#define IF_RTCD(x)  NULL
#endif

extern int VP8_UVSSE(MACROBLOCK *x, const vp8_variance_rtcd_vtable_t *rtcd);

#ifdef SPEEDSTATS
extern unsigned int cnt_pm;
#endif

extern const MV_REFERENCE_FRAME vp8_ref_frame_order[MAX_MODES];
extern const MB_PREDICTION_MODE vp8_mode_order[MAX_MODES];


extern unsigned int (*vp8_get16x16pred_error)(unsigned char *src_ptr, int src_stride, unsigned char *ref_ptr, int ref_stride);
extern unsigned int (*vp8_get4x4sse_cs)(unsigned char *src_ptr, int  source_stride, unsigned char *ref_ptr, int  recon_stride);
extern int vp8_rd_pick_best_mbsegmentation(VP8_COMP *cpi, MACROBLOCK *x, MV *best_ref_mv, int best_rd, int *, int *, int *, int, int *mvcost[2], int, int fullpixel);
extern int vp8_cost_mv_ref(MB_PREDICTION_MODE m, const int near_mv_ref_ct[4]);
extern void vp8_set_mbmode_and_mvs(MACROBLOCK *x, MB_PREDICTION_MODE mb, MV *mv);


int vp8_skip_fractional_mv_step(MACROBLOCK *mb, BLOCK *b, BLOCKD *d, MV *bestmv, MV *ref_mv, int error_per_bit, const vp8_variance_fn_ptr_t *vfp, int *mvcost[2])
{
    (void) b;
    (void) d;
    (void) ref_mv;
    (void) error_per_bit;
    (void) vfp;
    (void) mvcost;
    bestmv->row <<= 3;
    bestmv->col <<= 3;
    return 0;
}


static int get_inter_mbpred_error(MACROBLOCK *mb, const vp8_variance_fn_ptr_t *vfp, unsigned int *sse)
{

    BLOCK *b = &mb->block[0];
    BLOCKD *d = &mb->e_mbd.block[0];
    unsigned char *what = (*(b->base_src) + b->src);
    int what_stride = b->src_stride;
    unsigned char *in_what = *(d->base_pre) + d->pre ;
    int in_what_stride = d->pre_stride;
    int xoffset = d->bmi.mv.as_mv.col & 7;
    int yoffset = d->bmi.mv.as_mv.row & 7;

    in_what += (d->bmi.mv.as_mv.row >> 3) * d->pre_stride + (d->bmi.mv.as_mv.col >> 3);

    if (xoffset | yoffset)
    {
        return vfp->svf(in_what, in_what_stride, xoffset, yoffset, what, what_stride, sse);
    }
    else
    {
        return vfp->vf(what, what_stride, in_what, in_what_stride, sse);
    }

}

unsigned int vp8_get16x16pred_error_c
(
    const unsigned char *src_ptr,
    int src_stride,
    const unsigned char *ref_ptr,
    int ref_stride,
    int max_sad
)
{
    unsigned pred_error = 0;
    int i, j;
    int sum = 0;

    for (i = 0; i < 16; i++)
    {
        int diff;

        for (j = 0; j < 16; j++)
        {
            diff = src_ptr[j] - ref_ptr[j];
            sum += diff;
            pred_error += diff * diff;
        }

        src_ptr += src_stride;
        ref_ptr += ref_stride;
    }

    pred_error -= sum * sum / 256;
    return pred_error;
}


unsigned int vp8_get4x4sse_cs_c
(
    const unsigned char *src_ptr,
    int  source_stride,
    const unsigned char *ref_ptr,
    int  recon_stride,
    int max_sad
)
{
    int distortion = 0;
    int r, c;

    for (r = 0; r < 4; r++)
    {
        for (c = 0; c < 4; c++)
        {
            int diff = src_ptr[c] - ref_ptr[c];
            distortion += diff * diff;
        }

        src_ptr += source_stride;
        ref_ptr += recon_stride;
    }

    return distortion;
}

static int get_prediction_error(BLOCK *be, BLOCKD *b, const vp8_variance_rtcd_vtable_t *rtcd)
{
    unsigned char *sptr;
    unsigned char *dptr;
    sptr = (*(be->base_src) + be->src);
    dptr = b->predictor;

    return VARIANCE_INVOKE(rtcd, get4x4sse_cs)(sptr, be->src_stride, dptr, 16, 0x7fffffff);

}

static int pick_intra4x4block(
    const VP8_ENCODER_RTCD *rtcd,
    MACROBLOCK *x,
    BLOCK *be,
    BLOCKD *b,
    B_PREDICTION_MODE *best_mode,
    B_PREDICTION_MODE above,
    B_PREDICTION_MODE left,
    ENTROPY_CONTEXT *a,
    ENTROPY_CONTEXT *l,

    int *bestrate,
    int *bestdistortion)
{
    B_PREDICTION_MODE mode;
    int best_rd = INT_MAX;       // 1<<30
    int rate;
    int distortion;
    unsigned int *mode_costs;
    (void) l;
    (void) a;

    if (x->e_mbd.frame_type == KEY_FRAME)
    {
        mode_costs = x->bmode_costs[above][left];
    }
    else
    {
        mode_costs = x->inter_bmode_costs;
    }

    for (mode = B_DC_PRED; mode <= B_HE_PRED /*B_HU_PRED*/; mode++)
    {
        int this_rd;

        rate = mode_costs[mode];
        vp8_predict_intra4x4(b, mode, b->predictor);
        distortion = get_prediction_error(be, b, &rtcd->variance);
        this_rd = RD_ESTIMATE(x->rdmult, x->rddiv, rate, distortion);

        if (this_rd < best_rd)
        {
            *bestrate = rate;
            *bestdistortion = distortion;
            best_rd = this_rd;
            *best_mode = mode;
        }
    }

    b->bmi.mode = (B_PREDICTION_MODE)(*best_mode);
    vp8_encode_intra4x4block(rtcd, x, be, b, b->bmi.mode);
    return best_rd;
}


int vp8_pick_intra4x4mby_modes(const VP8_ENCODER_RTCD *rtcd, MACROBLOCK *mb, int *Rate, int *best_dist)
{
    MACROBLOCKD *const xd = &mb->e_mbd;
    int i;
    int cost = mb->mbmode_cost [xd->frame_type] [B_PRED];
    int error = RD_ESTIMATE(mb->rdmult, mb->rddiv, cost, 0); // Rd estimate for the cost of the block prediction mode
    int distortion = 0;
    ENTROPY_CONTEXT_PLANES t_above, t_left;
    ENTROPY_CONTEXT *ta;
    ENTROPY_CONTEXT *tl;

    vpx_memcpy(&t_above, mb->e_mbd.above_context, sizeof(ENTROPY_CONTEXT_PLANES));
    vpx_memcpy(&t_left, mb->e_mbd.left_context, sizeof(ENTROPY_CONTEXT_PLANES));

    ta = (ENTROPY_CONTEXT *)&t_above;
    tl = (ENTROPY_CONTEXT *)&t_left;

    vp8_intra_prediction_down_copy(xd);

    for (i = 0; i < 16; i++)
    {
        MODE_INFO *const mic = xd->mode_info_context;
        const int mis = xd->mode_info_stride;
        const B_PREDICTION_MODE A = vp8_above_bmi(mic, i, mis)->mode;
        const B_PREDICTION_MODE L = vp8_left_bmi(mic, i)->mode;
        B_PREDICTION_MODE UNINITIALIZED_IS_SAFE(best_mode);
        int UNINITIALIZED_IS_SAFE(r), UNINITIALIZED_IS_SAFE(d);

        error += pick_intra4x4block(rtcd,
                                    mb, mb->block + i, xd->block + i, &best_mode, A, L,
                                    ta + vp8_block2above[i],
                                    tl + vp8_block2left[i], &r, &d);

        cost += r;
        distortion += d;

        mic->bmi[i].mode = xd->block[i].bmi.mode = best_mode;

        // Break out case where we have already exceeded best so far value that was bassed in
        if (distortion > *best_dist)
            break;
    }

    for (i = 0; i < 16; i++)
        xd->block[i].bmi.mv.as_int = 0;

    *Rate = cost;

    if (i == 16)
        *best_dist = distortion;
    else
        *best_dist = INT_MAX;


    return error;
}

int vp8_pick_intra_mbuv_mode(MACROBLOCK *mb)
{

    MACROBLOCKD *x = &mb->e_mbd;
    unsigned char *uabove_row = x->dst.u_buffer - x->dst.uv_stride;
    unsigned char *vabove_row = x->dst.v_buffer - x->dst.uv_stride;
    unsigned char *usrc_ptr = (mb->block[16].src + *mb->block[16].base_src);
    unsigned char *vsrc_ptr = (mb->block[20].src + *mb->block[20].base_src);
    int uvsrc_stride = mb->block[16].src_stride;
    unsigned char uleft_col[8];
    unsigned char vleft_col[8];
    unsigned char utop_left = uabove_row[-1];
    unsigned char vtop_left = vabove_row[-1];
    int i, j;
    int expected_udc;
    int expected_vdc;
    int shift;
    int Uaverage = 0;
    int Vaverage = 0;
    int diff;
    int pred_error[4] = {0, 0, 0, 0}, best_error = INT_MAX;
    MB_PREDICTION_MODE UNINITIALIZED_IS_SAFE(best_mode);


    for (i = 0; i < 8; i++)
    {
        uleft_col[i] = x->dst.u_buffer [i* x->dst.uv_stride -1];
        vleft_col[i] = x->dst.v_buffer [i* x->dst.uv_stride -1];
    }

    if (!x->up_available && !x->left_available)
    {
        expected_udc = 128;
        expected_vdc = 128;
    }
    else
    {
        shift = 2;

        if (x->up_available)
        {

            for (i = 0; i < 8; i++)
            {
                Uaverage += uabove_row[i];
                Vaverage += vabove_row[i];
            }

            shift ++;

        }

        if (x->left_available)
        {
            for (i = 0; i < 8; i++)
            {
                Uaverage += uleft_col[i];
                Vaverage += vleft_col[i];
            }

            shift ++;

        }

        expected_udc = (Uaverage + (1 << (shift - 1))) >> shift;
        expected_vdc = (Vaverage + (1 << (shift - 1))) >> shift;
    }


    for (i = 0; i < 8; i++)
    {
        for (j = 0; j < 8; j++)
        {

            int predu = uleft_col[i] + uabove_row[j] - utop_left;
            int predv = vleft_col[i] + vabove_row[j] - vtop_left;
            int u_p, v_p;

            u_p = usrc_ptr[j];
            v_p = vsrc_ptr[j];

            if (predu < 0)
                predu = 0;

            if (predu > 255)
                predu = 255;

            if (predv < 0)
                predv = 0;

            if (predv > 255)
                predv = 255;


            diff = u_p - expected_udc;
            pred_error[DC_PRED] += diff * diff;
            diff = v_p - expected_vdc;
            pred_error[DC_PRED] += diff * diff;


            diff = u_p - uabove_row[j];
            pred_error[V_PRED] += diff * diff;
            diff = v_p - vabove_row[j];
            pred_error[V_PRED] += diff * diff;


            diff = u_p - uleft_col[i];
            pred_error[H_PRED] += diff * diff;
            diff = v_p - vleft_col[i];
            pred_error[H_PRED] += diff * diff;


            diff = u_p - predu;
            pred_error[TM_PRED] += diff * diff;
            diff = v_p - predv;
            pred_error[TM_PRED] += diff * diff;


        }

        usrc_ptr += uvsrc_stride;
        vsrc_ptr += uvsrc_stride;

        if (i == 3)
        {
            usrc_ptr = (mb->block[18].src + *mb->block[18].base_src);
            vsrc_ptr = (mb->block[22].src + *mb->block[22].base_src);
        }



    }


    for (i = DC_PRED; i <= TM_PRED; i++)
    {
        if (best_error > pred_error[i])
        {
            best_error = pred_error[i];
            best_mode = (MB_PREDICTION_MODE)i;
        }
    }


    mb->e_mbd.mode_info_context->mbmi.uv_mode = best_mode;
    return best_error;

}


int vp8_pick_inter_mode(VP8_COMP *cpi, MACROBLOCK *x, int recon_yoffset, int recon_uvoffset, int *returnrate, int *returndistortion, int *returnintra)
{
    BLOCK *b = &x->block[0];
    BLOCKD *d = &x->e_mbd.block[0];
    MACROBLOCKD *xd = &x->e_mbd;
    B_MODE_INFO best_bmodes[16];
    MB_MODE_INFO best_mbmode;
    PARTITION_INFO best_partition;
    MV best_ref_mv1;
    MV mode_mv[MB_MODE_COUNT];
    MB_PREDICTION_MODE this_mode;
    int num00;
    int i;
    int mdcounts[4];
    int best_rd = INT_MAX; // 1 << 30;
    int best_intra_rd = INT_MAX;
    int mode_index;
    int ref_frame_cost[MAX_REF_FRAMES];
    int rate;
    int rate2;
    int distortion2;
    int bestsme;
    //int all_rds[MAX_MODES];         // Experimental debug code.
    int best_mode_index = 0;
    int sse = INT_MAX;

    MV nearest_mv[4];
    MV near_mv[4];
    MV best_ref_mv[4];
    int MDCounts[4][4];
    unsigned char *y_buffer[4];
    unsigned char *u_buffer[4];
    unsigned char *v_buffer[4];

    int skip_mode[4] = {0, 0, 0, 0};

    vpx_memset(mode_mv, 0, sizeof(mode_mv));
    vpx_memset(nearest_mv, 0, sizeof(nearest_mv));
    vpx_memset(near_mv, 0, sizeof(near_mv));
    vpx_memset(&best_mbmode, 0, sizeof(best_mbmode));


    // set up all the refframe dependent pointers.
    if (cpi->ref_frame_flags & VP8_LAST_FLAG)
    {
        YV12_BUFFER_CONFIG *lst_yv12 = &cpi->common.yv12_fb[cpi->common.lst_fb_idx];

        vp8_find_near_mvs(&x->e_mbd, x->e_mbd.mode_info_context, &nearest_mv[LAST_FRAME], &near_mv[LAST_FRAME],
                          &best_ref_mv[LAST_FRAME], MDCounts[LAST_FRAME], LAST_FRAME, cpi->common.ref_frame_sign_bias);

        y_buffer[LAST_FRAME] = lst_yv12->y_buffer + recon_yoffset;
        u_buffer[LAST_FRAME] = lst_yv12->u_buffer + recon_uvoffset;
        v_buffer[LAST_FRAME] = lst_yv12->v_buffer + recon_uvoffset;
    }
    else
        skip_mode[LAST_FRAME] = 1;

    if (cpi->ref_frame_flags & VP8_GOLD_FLAG)
    {
        YV12_BUFFER_CONFIG *gld_yv12 = &cpi->common.yv12_fb[cpi->common.gld_fb_idx];

        vp8_find_near_mvs(&x->e_mbd, x->e_mbd.mode_info_context, &nearest_mv[GOLDEN_FRAME], &near_mv[GOLDEN_FRAME],
                          &best_ref_mv[GOLDEN_FRAME], MDCounts[GOLDEN_FRAME], GOLDEN_FRAME, cpi->common.ref_frame_sign_bias);

        y_buffer[GOLDEN_FRAME] = gld_yv12->y_buffer + recon_yoffset;
        u_buffer[GOLDEN_FRAME] = gld_yv12->u_buffer + recon_uvoffset;
        v_buffer[GOLDEN_FRAME] = gld_yv12->v_buffer + recon_uvoffset;
    }
    else
        skip_mode[GOLDEN_FRAME] = 1;

    if (cpi->ref_frame_flags & VP8_ALT_FLAG && cpi->source_alt_ref_active)
    {
        YV12_BUFFER_CONFIG *alt_yv12 = &cpi->common.yv12_fb[cpi->common.alt_fb_idx];

        vp8_find_near_mvs(&x->e_mbd, x->e_mbd.mode_info_context, &nearest_mv[ALTREF_FRAME], &near_mv[ALTREF_FRAME],
                          &best_ref_mv[ALTREF_FRAME], MDCounts[ALTREF_FRAME], ALTREF_FRAME, cpi->common.ref_frame_sign_bias);

        y_buffer[ALTREF_FRAME] = alt_yv12->y_buffer + recon_yoffset;
        u_buffer[ALTREF_FRAME] = alt_yv12->u_buffer + recon_uvoffset;
        v_buffer[ALTREF_FRAME] = alt_yv12->v_buffer + recon_uvoffset;
    }
    else
        skip_mode[ALTREF_FRAME] = 1;

    cpi->mbs_tested_so_far++;          // Count of the number of MBs tested so far this frame

    *returnintra = best_intra_rd;
    x->skip = 0;

    ref_frame_cost[INTRA_FRAME]   = vp8_cost_zero(cpi->prob_intra_coded);

    // Special case treatment when GF and ARF are not sensible options for reference
    if (cpi->ref_frame_flags == VP8_LAST_FLAG)
    {
        ref_frame_cost[LAST_FRAME]    = vp8_cost_one(cpi->prob_intra_coded)
                                        + vp8_cost_zero(255);
        ref_frame_cost[GOLDEN_FRAME]  = vp8_cost_one(cpi->prob_intra_coded)
                                        + vp8_cost_one(255)
                                        + vp8_cost_zero(128);
        ref_frame_cost[ALTREF_FRAME]  = vp8_cost_one(cpi->prob_intra_coded)
                                        + vp8_cost_one(255)
                                        + vp8_cost_one(128);
    }
    else
    {
        ref_frame_cost[LAST_FRAME]    = vp8_cost_one(cpi->prob_intra_coded)
                                        + vp8_cost_zero(cpi->prob_last_coded);
        ref_frame_cost[GOLDEN_FRAME]  = vp8_cost_one(cpi->prob_intra_coded)
                                        + vp8_cost_one(cpi->prob_last_coded)
                                        + vp8_cost_zero(cpi->prob_gf_coded);
        ref_frame_cost[ALTREF_FRAME]  = vp8_cost_one(cpi->prob_intra_coded)
                                        + vp8_cost_one(cpi->prob_last_coded)
                                        + vp8_cost_one(cpi->prob_gf_coded);
    }



    best_rd = INT_MAX;

    x->e_mbd.mode_info_context->mbmi.ref_frame = INTRA_FRAME;

    // if we encode a new mv this is important
    // find the best new motion vector
    for (mode_index = 0; mode_index < MAX_MODES; mode_index++)
    {
        int frame_cost;
        int this_rd = INT_MAX;

        if (best_rd <= cpi->rd_threshes[mode_index])
            continue;

        x->e_mbd.mode_info_context->mbmi.ref_frame = vp8_ref_frame_order[mode_index];

        if (skip_mode[x->e_mbd.mode_info_context->mbmi.ref_frame])
            continue;

        // Check to see if the testing frequency for this mode is at its max
        // If so then prevent it from being tested and increase the threshold for its testing
        if (cpi->mode_test_hit_counts[mode_index] && (cpi->mode_check_freq[mode_index] > 1))
        {
            //if ( (cpi->mbs_tested_so_far / cpi->mode_test_hit_counts[mode_index]) <= cpi->mode_check_freq[mode_index] )
            if (cpi->mbs_tested_so_far <= (cpi->mode_check_freq[mode_index] * cpi->mode_test_hit_counts[mode_index]))
            {
                // Increase the threshold for coding this mode to make it less likely to be chosen
                cpi->rd_thresh_mult[mode_index] += 4;

                if (cpi->rd_thresh_mult[mode_index] > MAX_THRESHMULT)
                    cpi->rd_thresh_mult[mode_index] = MAX_THRESHMULT;

                cpi->rd_threshes[mode_index] = (cpi->rd_baseline_thresh[mode_index] >> 7) * cpi->rd_thresh_mult[mode_index];

                continue;
            }
        }

        // We have now reached the point where we are going to test the current mode so increment the counter for the number of times it has been tested
        cpi->mode_test_hit_counts[mode_index] ++;

        rate2 = 0;
        distortion2 = 0;

        this_mode = vp8_mode_order[mode_index];

        // Experimental debug code.
        //all_rds[mode_index] = -1;

        x->e_mbd.mode_info_context->mbmi.mode = this_mode;
        x->e_mbd.mode_info_context->mbmi.uv_mode = DC_PRED;

        // Work out the cost assosciated with selecting the reference frame
        frame_cost = ref_frame_cost[x->e_mbd.mode_info_context->mbmi.ref_frame];
        rate2 += frame_cost;

        // everything but intra
        if (x->e_mbd.mode_info_context->mbmi.ref_frame)
        {
            x->e_mbd.pre.y_buffer = y_buffer[x->e_mbd.mode_info_context->mbmi.ref_frame];
            x->e_mbd.pre.u_buffer = u_buffer[x->e_mbd.mode_info_context->mbmi.ref_frame];
            x->e_mbd.pre.v_buffer = v_buffer[x->e_mbd.mode_info_context->mbmi.ref_frame];
            mode_mv[NEARESTMV] = nearest_mv[x->e_mbd.mode_info_context->mbmi.ref_frame];
            mode_mv[NEARMV] = near_mv[x->e_mbd.mode_info_context->mbmi.ref_frame];
            best_ref_mv1 = best_ref_mv[x->e_mbd.mode_info_context->mbmi.ref_frame];
            memcpy(mdcounts, MDCounts[x->e_mbd.mode_info_context->mbmi.ref_frame], sizeof(mdcounts));
        }

        //Only consider ZEROMV/ALTREF_FRAME for alt ref frame.
        if (cpi->is_src_frame_alt_ref)
        {
            if (this_mode != ZEROMV || x->e_mbd.mode_info_context->mbmi.ref_frame != ALTREF_FRAME)
                continue;
        }

        switch (this_mode)
        {
        case B_PRED:
            distortion2 = *returndistortion;                    // Best so far passed in as breakout value to vp8_pick_intra4x4mby_modes
            vp8_pick_intra4x4mby_modes(IF_RTCD(&cpi->rtcd), x, &rate, &distortion2);
            rate2 += rate;
            distortion2 = VARIANCE_INVOKE(&cpi->rtcd.variance, get16x16prederror)(x->src.y_buffer, x->src.y_stride, x->e_mbd.predictor, 16, 0x7fffffff);

            if (distortion2 == INT_MAX)
            {
                this_rd = INT_MAX;
            }
            else
            {
                this_rd = RD_ESTIMATE(x->rdmult, x->rddiv, rate2, distortion2);

                if (this_rd < best_intra_rd)
                {
                    best_intra_rd = this_rd;
                    *returnintra = best_intra_rd ;
                }
            }

            break;

        case SPLITMV:

            // Split MV modes currently not supported when RD is nopt enabled.
            break;

        case DC_PRED:
        case V_PRED:
        case H_PRED:
        case TM_PRED:
            vp8_build_intra_predictors_mby_ptr(&x->e_mbd);
            distortion2 = VARIANCE_INVOKE(&cpi->rtcd.variance, get16x16prederror)(x->src.y_buffer, x->src.y_stride, x->e_mbd.predictor, 16, 0x7fffffff);
            rate2 += x->mbmode_cost[x->e_mbd.frame_type][x->e_mbd.mode_info_context->mbmi.mode];
            this_rd = RD_ESTIMATE(x->rdmult, x->rddiv, rate2, distortion2);

            if (this_rd < best_intra_rd)
            {
                best_intra_rd = this_rd;
                *returnintra = best_intra_rd ;
            }

            break;

        case NEWMV:
        {
            int thissme;
            int step_param;
            int further_steps;
            int n = 0;
            int sadpb = x->sadperbit16;

            // Further step/diamond searches as necessary
            if (cpi->Speed < 8)
            {
                step_param = cpi->sf.first_step + ((cpi->Speed > 5) ? 1 : 0);
                further_steps = (cpi->sf.max_step_search_steps - 1) - step_param;
            }
            else
            {
                step_param = cpi->sf.first_step + 2;
                further_steps = 0;
            }

#if 0

            // Initial step Search
            bestsme = vp8_diamond_search_sad(x, b, d, &best_ref_mv1, &d->bmi.mv.as_mv, step_param, x->errorperbit, &num00, &cpi->fn_ptr, cpi->mb.mvsadcost, cpi->mb.mvcost, &best_ref_mv1);
            mode_mv[NEWMV].row = d->bmi.mv.as_mv.row;
            mode_mv[NEWMV].col = d->bmi.mv.as_mv.col;

            // Further step searches
            while (n < further_steps)
            {
                n++;

                if (num00)
                    num00--;
                else
                {
                    thissme = vp8_diamond_search_sad(x, b, d, &best_ref_mv1, &d->bmi.mv.as_mv, step_param + n, x->errorperbit, &num00, &cpi->fn_ptr, cpi->mb.mvsadcost, x->mvcost, &best_ref_mv1);

                    if (thissme < bestsme)
                    {
                        bestsme = thissme;
                        mode_mv[NEWMV].row = d->bmi.mv.as_mv.row;
                        mode_mv[NEWMV].col = d->bmi.mv.as_mv.col;
                    }
                    else
                    {
                        d->bmi.mv.as_mv.row = mode_mv[NEWMV].row;
                        d->bmi.mv.as_mv.col = mode_mv[NEWMV].col;
                    }
                }
            }

#else

            if (cpi->sf.search_method == HEX)
            {
                bestsme = vp8_hex_search(x, b, d, &best_ref_mv1, &d->bmi.mv.as_mv, step_param, sadpb/*x->errorperbit*/, &num00, &cpi->fn_ptr[BLOCK_16X16], x->mvsadcost, x->mvcost);
                mode_mv[NEWMV].row = d->bmi.mv.as_mv.row;
                mode_mv[NEWMV].col = d->bmi.mv.as_mv.col;
            }
            else
            {
                bestsme = cpi->diamond_search_sad(x, b, d, &best_ref_mv1, &d->bmi.mv.as_mv, step_param, sadpb / 2/*x->errorperbit*/, &num00, &cpi->fn_ptr[BLOCK_16X16], x->mvsadcost, x->mvcost, &best_ref_mv1); //sadpb < 9
                mode_mv[NEWMV].row = d->bmi.mv.as_mv.row;
                mode_mv[NEWMV].col = d->bmi.mv.as_mv.col;

                // Further step/diamond searches as necessary
                n = 0;
                //further_steps = (cpi->sf.max_step_search_steps - 1) - step_param;

                n = num00;
                num00 = 0;

                while (n < further_steps)
                {
                    n++;

                    if (num00)
                        num00--;
                    else
                    {
                        thissme = cpi->diamond_search_sad(x, b, d, &best_ref_mv1, &d->bmi.mv.as_mv, step_param + n, sadpb / 4/*x->errorperbit*/, &num00, &cpi->fn_ptr[BLOCK_16X16], x->mvsadcost, x->mvcost, &best_ref_mv1); //sadpb = 9

                        if (thissme < bestsme)
                        {
                            bestsme = thissme;
                            mode_mv[NEWMV].row = d->bmi.mv.as_mv.row;
                            mode_mv[NEWMV].col = d->bmi.mv.as_mv.col;
                        }
                        else
                        {
                            d->bmi.mv.as_mv.row = mode_mv[NEWMV].row;
                            d->bmi.mv.as_mv.col = mode_mv[NEWMV].col;
                        }
                    }
                }
            }

#endif
        }

        if (bestsme < INT_MAX)
            cpi->find_fractional_mv_step(x, b, d, &d->bmi.mv.as_mv, &best_ref_mv1, x->errorperbit, &cpi->fn_ptr[BLOCK_16X16], cpi->mb.mvcost);

        mode_mv[NEWMV].row = d->bmi.mv.as_mv.row;
        mode_mv[NEWMV].col = d->bmi.mv.as_mv.col;

        // mv cost;
        rate2 += vp8_mv_bit_cost(&mode_mv[NEWMV], &best_ref_mv1, cpi->mb.mvcost, 128);


        case NEARESTMV:
        case NEARMV:

            if (mode_mv[this_mode].row == 0 && mode_mv[this_mode].col == 0)
                continue;

        case ZEROMV:

            // Trap vectors that reach beyond the UMV borders
            // Note that ALL New MV, Nearest MV Near MV and Zero MV code drops through to this point
            // because of the lack of break statements in the previous two cases.
            if (((mode_mv[this_mode].row >> 3) < x->mv_row_min) || ((mode_mv[this_mode].row >> 3) > x->mv_row_max) ||
                ((mode_mv[this_mode].col >> 3) < x->mv_col_min) || ((mode_mv[this_mode].col >> 3) > x->mv_col_max))
                continue;

            rate2 += vp8_cost_mv_ref(this_mode, mdcounts);
            x->e_mbd.mode_info_context->mbmi.mode = this_mode;
            x->e_mbd.mode_info_context->mbmi.mv.as_mv = mode_mv[this_mode];
            x->e_mbd.block[0].bmi.mode = this_mode;
            x->e_mbd.block[0].bmi.mv.as_int = x->e_mbd.mode_info_context->mbmi.mv.as_int;

            distortion2 = get_inter_mbpred_error(x, &cpi->fn_ptr[BLOCK_16X16], (unsigned int *)(&sse));

            this_rd = RD_ESTIMATE(x->rdmult, x->rddiv, rate2, distortion2);

            if (cpi->active_map_enabled && x->active_ptr[0] == 0)
            {
                x->skip = 1;
            }
            else if (sse < x->encode_breakout)
            {
                // Check u and v to make sure skip is ok
                int sse2 = 0;

                sse2 = VP8_UVSSE(x, IF_RTCD(&cpi->rtcd.variance));

                if (sse2 * 2 < x->encode_breakout)
                    x->skip = 1;
                else
                    x->skip = 0;
            }

            break;
        default:
            break;
        }

        // Experimental debug code.
        //all_rds[mode_index] = this_rd;

        if (this_rd < best_rd || x->skip)
        {
            // Note index of best mode
            best_mode_index = mode_index;

            *returnrate = rate2;
            *returndistortion = distortion2;
            best_rd = this_rd;
            vpx_memcpy(&best_mbmode, &x->e_mbd.mode_info_context->mbmi, sizeof(MB_MODE_INFO));
            vpx_memcpy(&best_partition, x->partition_info, sizeof(PARTITION_INFO));

            if (this_mode == B_PRED || this_mode == SPLITMV)
                for (i = 0; i < 16; i++)
                {
                    vpx_memcpy(&best_bmodes[i], &x->e_mbd.block[i].bmi, sizeof(B_MODE_INFO));
                }
            else
            {
                best_bmodes[0].mv = x->e_mbd.block[0].bmi.mv;
            }

            // Testing this mode gave rise to an improvement in best error score. Lower threshold a bit for next time
            cpi->rd_thresh_mult[mode_index] = (cpi->rd_thresh_mult[mode_index] >= (MIN_THRESHMULT + 2)) ? cpi->rd_thresh_mult[mode_index] - 2 : MIN_THRESHMULT;
            cpi->rd_threshes[mode_index] = (cpi->rd_baseline_thresh[mode_index] >> 7) * cpi->rd_thresh_mult[mode_index];
        }

        // If the mode did not help improve the best error case then raise the threshold for testing that mode next time around.
        else
        {
            cpi->rd_thresh_mult[mode_index] += 4;

            if (cpi->rd_thresh_mult[mode_index] > MAX_THRESHMULT)
                cpi->rd_thresh_mult[mode_index] = MAX_THRESHMULT;

            cpi->rd_threshes[mode_index] = (cpi->rd_baseline_thresh[mode_index] >> 7) * cpi->rd_thresh_mult[mode_index];
        }

        if (x->skip)
            break;
    }

    // Reduce the activation RD thresholds for the best choice mode
    if ((cpi->rd_baseline_thresh[best_mode_index] > 0) && (cpi->rd_baseline_thresh[best_mode_index] < (INT_MAX >> 2)))
    {
        int best_adjustment = (cpi->rd_thresh_mult[best_mode_index] >> 3);

        cpi->rd_thresh_mult[best_mode_index] = (cpi->rd_thresh_mult[best_mode_index] >= (MIN_THRESHMULT + best_adjustment)) ? cpi->rd_thresh_mult[best_mode_index] - best_adjustment : MIN_THRESHMULT;
        cpi->rd_threshes[best_mode_index] = (cpi->rd_baseline_thresh[best_mode_index] >> 7) * cpi->rd_thresh_mult[best_mode_index];
    }

    // Keep a record of best mode index for use in next loop
    cpi->last_best_mode_index = best_mode_index;

    if (best_mbmode.mode <= B_PRED)
    {
        x->e_mbd.mode_info_context->mbmi.ref_frame = INTRA_FRAME;
        vp8_pick_intra_mbuv_mode(x);
        best_mbmode.uv_mode = x->e_mbd.mode_info_context->mbmi.uv_mode;
    }


    {
        int this_rdbin = (*returndistortion >> 7);

        if (this_rdbin >= 1024)
        {
            this_rdbin = 1023;
        }

        cpi->error_bins[this_rdbin] ++;
    }


    if (cpi->is_src_frame_alt_ref && (best_mbmode.mode != ZEROMV || best_mbmode.ref_frame != ALTREF_FRAME))
    {
        best_mbmode.mode = ZEROMV;
        best_mbmode.ref_frame = ALTREF_FRAME;
        best_mbmode.mv.as_int = 0;
        best_mbmode.uv_mode = 0;
        best_mbmode.mb_skip_coeff = (cpi->common.mb_no_coeff_skip) ? 1 : 0;
        best_mbmode.partitioning = 0;
        best_mbmode.dc_diff = 0;

        vpx_memcpy(&x->e_mbd.mode_info_context->mbmi, &best_mbmode, sizeof(MB_MODE_INFO));
        vpx_memcpy(x->partition_info, &best_partition, sizeof(PARTITION_INFO));

        for (i = 0; i < 16; i++)
        {
            vpx_memset(&x->e_mbd.block[i].bmi, 0, sizeof(B_MODE_INFO));
        }

        x->e_mbd.mode_info_context->mbmi.mv.as_int = 0;

        return best_rd;
    }


    // macroblock modes
    vpx_memcpy(&x->e_mbd.mode_info_context->mbmi, &best_mbmode, sizeof(MB_MODE_INFO));
    vpx_memcpy(x->partition_info, &best_partition, sizeof(PARTITION_INFO));

    if (x->e_mbd.mode_info_context->mbmi.mode == B_PRED || x->e_mbd.mode_info_context->mbmi.mode == SPLITMV)
        for (i = 0; i < 16; i++)
        {
            vpx_memcpy(&x->e_mbd.block[i].bmi, &best_bmodes[i], sizeof(B_MODE_INFO));

        }
    else
    {
        vp8_set_mbmode_and_mvs(x, x->e_mbd.mode_info_context->mbmi.mode, &best_bmodes[0].mv.as_mv);
    }

    x->e_mbd.mode_info_context->mbmi.mv.as_mv = x->e_mbd.block[15].bmi.mv.as_mv;

    return best_rd;
}
