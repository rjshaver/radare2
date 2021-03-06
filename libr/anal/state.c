/* radare - Apache 2.0 - Copyright 2013 - Adam Pridgen <dso@rice.edu || adam.pridgen@thecoverofnight.com> */

#include <r_anal.h>
#include <r_util.h>
#include <r_list.h>
#include <r_io.h>

#ifdef IFDBG
#undef IFDBG
#endif

#define DO_THE_DBG 0
#define IFDBG if(DO_THE_DBG)
#define IFINT if(0)

static void kv_anal_bb_free(HtUPKv *kv) {
	r_anal_block_unref (kv->value);
}

R_API RAnalState * r_anal_state_new(ut64 start, ut8* buffer, ut64 len) {
	RAnalState *state = R_NEW0 (RAnalState);
	if (!state) {
		return NULL;
	}
	state->start = start;
	state->end = start + len;
	state->buffer = buffer;
	state->len = len;
	state->current_op = NULL;
	state->current_bb = NULL;
	state->current_fcn = NULL;
	state->ht = ht_up_new (NULL, (HtUPKvFreeFunc)kv_anal_bb_free, NULL);
	state->ht_sz = 512;
	state->bbs = r_list_newf ((RListFree)r_anal_block_unref);
	state->max_depth = 50;
	state->current_depth = 0;
	return state;
}

R_API void r_anal_state_set_depth(RAnalState *state, ut32 depth) {
	state->current_depth = depth;
}

R_API void r_anal_state_insert_bb(RAnalState* state, RAnalBlock *bb) {
	if (!state || !bb) {
		return;
	}
	if (!r_anal_state_search_bb (state, bb->addr) && state->current_fcn) {
		r_anal_function_add_block (state->current_fcn, bb);
		state->bytes_consumed += state->current_bb->op_sz;
		r_anal_block_ref (bb);
		if (!ht_up_insert (state->ht, bb->addr, bb)) {
			r_anal_block_unref (bb);
			eprintf ("Inserted bb 0x%04"PFMT64x" failure\n", bb->addr);
		}
	}
}

R_API RAnalBlock *r_anal_state_search_bb(RAnalState* state, ut64 addr) {
	return ht_up_find (state->ht, addr, NULL);
}

R_API void r_anal_state_free(RAnalState * state) {
	r_list_free (state->bbs);
	ht_up_free (state->ht);
	free (state);
}

R_API ut64 r_anal_state_get_len(RAnalState *state, ut64 addr) {
	ut64 result = 0;
	if (r_anal_state_addr_is_valid (state, addr)) {
		result = state->len - (addr - state->start);
	}
	return result;
}

R_API const ut8 * r_anal_state_get_buf_by_addr(RAnalState *state, ut64 addr) {
	if (r_anal_state_addr_is_valid (state, addr)) {
		ut64 offset = addr - state->start;
		return state->buffer + offset;
	}
	return NULL;
}

R_API bool r_anal_state_addr_is_valid(RAnalState *state, ut64 addr) {
	return (addr < state->end  && addr >= state->start);
}

R_API void r_anal_state_merge_bb_list (RAnalState *state, RList* bbs) {
	RListIter *iter;
	RAnalBlock *bb;
	r_list_foreach (bbs, iter, bb) {
		IFDBG eprintf ("Inserting bb from 0x%04"PFMT64x"\n", bb->addr);
		r_anal_state_insert_bb (state, bb);
	}
}
