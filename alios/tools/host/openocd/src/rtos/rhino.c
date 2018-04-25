/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <helper/time_support.h>
#include <jtag/jtag.h>
#include "target/target.h"
#include "target/target_type.h"
#include "rtos.h"
#include "helper/log.h"
#include "helper/types.h"
#include "rtos_standard_stackings.h"
#include "target/armv7m.h"
#include "target/cortex_m.h"


#define RHINO_LIST_OFFSET           36

typedef enum {
    K_SEED,
    K_RDY,
    K_PEND,
    K_SUSPENDED,
    K_PEND_SUSPENDED,
    K_SLEEP,
    K_SLEEP_SUSPENDED,
    K_DELETED,
} task_stat_t;

typedef enum {
    RHINO_SUCCESS                      = 0u,
    RHINO_SYS_FATAL_ERR,
    RHINO_SYS_SP_ERR,
    RHINO_RUNNING,
    RHINO_STOPPED
}kstat_t;


static int rhino_detect_rtos(struct target *target);
static int rhino_create(struct target *target);
static int rhino_update_threads(struct rtos *rtos);
static int rhino_get_thread_reg_list(struct rtos *rtos, int64_t thread_id, char **hex_reg_list);
static int rhino_get_symbol_list_to_lookup(symbol_table_elem_t *symbol_list[]);
static const struct rtos_register_stacking *get_stacking_info_arm966e(const struct rtos *rtos, int64_t stack_ptr);
static const struct rtos_register_stacking *get_stacking_info_armM(const struct rtos *rtos, int64_t stack_ptr);
static int64_t rhino_arm9_stack_calc(struct target *target,
	const uint8_t *stack_data, const struct rtos_register_stacking *stacking,
	int64_t stack_ptr);


struct rhino_thread_state {
	int value;
	const char *desc;
};

static const struct rhino_thread_state rhino_thread_states[] = {
    { K_SEED,            "Seed" },
    { K_RDY,             "Ready" },
    { K_PEND,            "Pend" },
    { K_SUSPENDED,       "Suspended" },
    { K_PEND_SUSPENDED,  "Pend_suspended" },
    { K_SLEEP,           "Sleep" },
    { K_SLEEP_SUSPENDED, "Sleep_suspended" },
    { K_DELETED,         "Deleted" }
};

#define ECOS_NUM_STATES (sizeof(rhino_thread_states)/sizeof(struct rhino_thread_state))


#define ARM966E_REGISTERS_SIZE_SOLICITED (17 * 4)
static const struct stack_register_offset rtos_rhnio_arm966e_stack_offsets_solicited[] = {
#if 1
    { 0x04, 32 },		/* r0        */
	{ 0x08, 32 },		/* r1        */
	{ 0x0C, 32 },		/* r2        */
	{ 0x10, 32 },		/* r3        */
	{ 0x14, 32 },		/* r4        */
	{ 0x18, 32 },		/* r5        */
	{ 0x1C, 32 },		/* r6        */
	{ 0x20, 32 },		/* r7        */
	{ 0x24, 32 },		/* r8        */
	{ 0x28, 32 },		/* r9        */
	{ 0x2C, 32 },		/* r10       */
	{ 0x30, 32 },		/* r11       */
	{ 0x34, 32 },		/* r12       */
	{ -2,   32 },		/* sp (r13)  */
	{ 0x38, 32 },		/* lr (r14)  */
	{ 0x3C, 32 },		/* pc (r15)  */
	{ 0x00, 32 },		/* xPSR      */
#else
	{ 0x08, 32 },		/* r0        */
	{ 0x0C, 32 },		/* r1        */
	{ 0x10, 32 },		/* r2        */
	{ 0x14, 32 },		/* r3        */
	{ 0x18, 32 },		/* r4        */
	{ 0x1C, 32 },		/* r5        */
	{ 0x20, 32 },		/* r6        */
	{ 0x24, 32 },		/* r7        */
	{ 0x28, 32 },		/* r8        */
	{ 0x2C, 32 },		/* r9        */
	{ 0x30, 32 },		/* r10       */
	{ 0x34, 32 },		/* r11       */
	{ 0x38, 32 },		/* r12       */
	{ -2,   32 },		/* sp (r13)  */
	{ 0x3C, 32 },		/* lr (r14)  */
	{ 0x40, 32 },		/* pc (r15)  */
	{ 0x04, 32 },		/* xPSR      */

#endif
};
#define ARM966E_REGISTERS_SIZE_INTERRUPT (17 * 4)
static const struct stack_register_offset rtos_rhnio_arm966e_stack_offsets_interrupt[] = {
	{ 0x08, 32 },		/* r0        */
	{ 0x0C, 32 },		/* r1        */
	{ 0x10, 32 },		/* r2        */
	{ 0x14, 32 },		/* r3        */
	{ 0x18, 32 },		/* r4        */
	{ 0x1C, 32 },		/* r5        */
	{ 0x20, 32 },		/* r6        */
	{ 0x24, 32 },		/* r7        */
	{ 0x28, 32 },		/* r8        */
	{ 0x2C, 32 },		/* r9        */
	{ 0x30, 32 },		/* r10       */
	{ 0x34, 32 },		/* r11       */
	{ 0x38, 32 },		/* r12       */
	{ -2,   32 },		/* sp (r13)  */
	{ 0x3C, 32 },		/* lr (r14)  */
	{ 0x40, 32 },		/* pc (r15)  */
	{ 0x04, 32 },		/* xPSR      */
};


const struct rtos_register_stacking rtos_rhnio_arm966e_stacking_solicited=
{
    ARM966E_REGISTERS_SIZE_SOLICITED,   /* stack_registers_size */
    -1,                                 /* stack_growth_direction */
    17,                                 /* num_output_registers */
    rhino_arm9_stack_calc,                               /* stack_alignment */
    rtos_rhnio_arm966e_stack_offsets_solicited  /* register_offsets */
};

const struct rtos_register_stacking rtos_rhnio_arm966e_stacking_interrupt=
{
	ARM966E_REGISTERS_SIZE_INTERRUPT,	/* stack_registers_size */
	-1,									/* stack_growth_direction */
	17,									/* num_output_registers */
	rhino_arm9_stack_calc,								/* stack_alignment */
	rtos_rhnio_arm966e_stack_offsets_interrupt	/* register_offsets */
};

const struct rtos_register_stacking *rtos_rhnio_arm966e_stacking[] = {
    &rtos_rhnio_arm966e_stacking_solicited,
    &rtos_rhnio_arm966e_stacking_interrupt,

};

const struct rtos_register_stacking *rtos_rhnio_armMx_stacking[] = {
    &rtos_standard_Cortex_M3_stacking,
    &rtos_standard_Cortex_M4F_stacking,
    &rtos_standard_Cortex_M4F_FPU_stacking,
};


struct rhino_params {
	const char *target_name;
	unsigned char pointer_width;
	unsigned char thread_stack_offset;
	unsigned char thread_name_offset;
	unsigned char thread_state_offset;
	const struct rtos_register_stacking **stacking_info;
	size_t stacking_info_nb;
	const struct rtos_register_stacking* (*fn_get_stacking_info)(const struct rtos *rtos, int64_t stack_ptr);
};

static const struct rhino_params rhino_params_list[] = {
    {
        "cortex_m",                        /* target_name */
        4,                                 /* pointer_width; */
        0,                                 /* thread_stack_offset; */
        72,                                /* thread_name_offset; */
        76,                                /* thread_state_offset; */
        &rtos_rhnio_armMx_stacking[0],        /* stacking_info */
        3,
        get_stacking_info_armM,
    },
    {
        "hla_target",                      /* target_name */
        4,                                 /* pointer_width; */
        0,                                 /* thread_stack_offset; */
        72,                                /* thread_name_offset; */
        76,                                /* thread_state_offset; */
        &rtos_rhnio_armMx_stacking[0],        /* stacking_info */
        3,
        get_stacking_info_armM,
    },
    {
        "arm966e",                          /* target_name */
        4,                                  /* pointer_width; */
        0,                                  /* thread_stack_offset; */
        72,                                 /* thread_name_offset; */
        76,                                 /* thread_state_offset; */
        &rtos_rhnio_arm966e_stacking[0],       /* stacking_info */
        2,
        get_stacking_info_arm966e,
    }
};

#define ECOS_NUM_PARAMS ((int)(sizeof(rhino_params_list)/sizeof(struct rhino_params)))

enum rhino_symbol_values {
    rhino_VAL_current_thread_ptr = 0,
    rhino_VAL_thread_list        = 1,
    rhino_VAL_sys_stat           = 2,
};

static const char * const rhino_symbol_list[] = {
    "g_active_task",
    "g_kobj_list",
    "g_sys_stat",
    NULL
};

const struct rtos_type rhino_rtos = {
	.name = "rhino",

	.detect_rtos = rhino_detect_rtos,
	.create = rhino_create,
	.update_threads = rhino_update_threads,
	.get_thread_reg_list = rhino_get_thread_reg_list,
	.get_symbol_list_to_lookup = rhino_get_symbol_list_to_lookup,

};

static const struct rtos_register_stacking *get_stacking_info_arm966e(const struct rtos *rtos, int64_t stack_ptr)
{
	const struct rhino_params *param = (const struct rhino_params *) rtos->rtos_specific_params;
	int	retval;
	uint32_t flag;

	retval = target_read_buffer(rtos->target,
			stack_ptr,
			sizeof(flag),
			(uint8_t *)&flag);
	if (retval != ERROR_OK) {
		LOG_ERROR("Error reading stack data from rhino thread: stack_ptr=0x%" PRIx64, stack_ptr);
		return NULL;
	}
	if ((flag & 0x13) == 0x13) {
		LOG_DEBUG("  solicited stack");
		return param->stacking_info[0];
	} else {
		LOG_DEBUG("  interrupt stack: %u", flag);
		return param->stacking_info[1];
	}
}

static const struct rtos_register_stacking *get_stacking_info_armM(const struct rtos *rtos, int64_t stack_ptr)
{
	const struct rhino_params *param = (const struct rhino_params *) rtos->rtos_specific_params;
	int	retval;


    /* Check for armv7m with *enabled* FPU, i.e. a Cortex-M4F */
    int cm4_fpu_enabled = 0;
    struct armv7m_common *armv7m_target = target_to_armv7m(rtos->target);
    if (is_armv7m(armv7m_target)) {
        if (armv7m_target->fp_feature == FPv4_SP) {
            /* Found ARM v7m target which includes a FPU */
            uint32_t cpacr;

            retval = target_read_u32(rtos->target, FPU_CPACR, &cpacr);
            if (retval != ERROR_OK) {
                LOG_ERROR("Could not read CPACR register to check FPU state");
                return NULL;
            }

            /* Check if CP10 and CP11 are set to full access. */
            if (cpacr & 0x00F00000) {
                /* Found target with enabled FPU */
                cm4_fpu_enabled = 1;
            }
        }
    }

    if (cm4_fpu_enabled == 1) {
        /* Read the LR to decide between stacking with or without FPU */
        uint32_t LR_svc = 0;
        retval = target_read_buffer(rtos->target,
                stack_ptr + 0x20,
                param->pointer_width,
                (uint8_t *)&LR_svc);
        if (retval != ERROR_OK) {
            LOG_OUTPUT("Error reading stack frame from FreeRTOS thread\r\n");
            return NULL;
        }
        if ((LR_svc & 0x10) == 0)
            return param->stacking_info[0];
        else
            return param->stacking_info[1];
    } else
        return param->stacking_info[2];


}

static const struct rtos_register_stacking *get_stacking_info(const struct rtos *rtos, int64_t stack_ptr)
{
	const struct rhino_params *param = (const struct rhino_params *) rtos->rtos_specific_params;

	if (param->fn_get_stacking_info != NULL)
		return param->fn_get_stacking_info(rtos, stack_ptr);

	return param->stacking_info[0];
}

static int64_t rhino_arm9_stack_calc(struct target *target,
	const uint8_t *stack_data, const struct rtos_register_stacking *stacking,
	int64_t stack_ptr)
{
	int64_t new_stack_ptr;

	new_stack_ptr = stack_ptr - stacking->stack_growth_direction *
		(stacking->stack_registers_size - 4);
	return new_stack_ptr;
}

static int rhino_update_threads(struct rtos *rtos)
{
	int retval;
	int tasks_found = 0;
	int thread_list_size = 0;
	const struct rhino_params *param;

	if (rtos == NULL)
		return -1;

	if (rtos->rtos_specific_params == NULL)
		return -3;

	param = (const struct rhino_params *) rtos->rtos_specific_params;

	if (rtos->symbols == NULL) {
		LOG_ERROR("No symbols for rhino");
		return -4;
	}

	if (rtos->symbols[rhino_VAL_thread_list].address == 0) {
		LOG_ERROR("Don't have the thread list head");
		return -2;
	}

	/* wipe out previous thread details if any */
	rtos_free_threadlist(rtos);

	/* read current system status */
	rtos->sys_stat = 0;
	retval = target_read_buffer(rtos->target,
			rtos->symbols[rhino_VAL_sys_stat].address,
			4,
			(uint8_t *)&rtos->sys_stat);
	if (retval != ERROR_OK) {
		LOG_ERROR("Could not read rhino current thread from target");
		return retval;
	}
    //if system is not runing, no thread information
    if(rtos->sys_stat != RHINO_RUNNING){
        return ERROR_OK;
    }

	/* determine the number of current threads */
	uint32_t thread_list_head = rtos->symbols[rhino_VAL_thread_list].address;
	uint32_t thread_list_next = 0;
	target_read_buffer(rtos->target,
		thread_list_head,
		param->pointer_width,
		(uint8_t *) &thread_list_next);
	while (thread_list_next && thread_list_next != thread_list_head) {
		thread_list_size++;
		retval = target_read_buffer(rtos->target,
				thread_list_next,
				param->pointer_width,
				(uint8_t *) &thread_list_next);
		if (retval != ERROR_OK)
			return retval;
	}

	/* read the current thread id */
	rtos->current_thread = 0;
	retval = target_read_buffer(rtos->target,
			rtos->symbols[rhino_VAL_current_thread_ptr].address,
			4,
			(uint8_t *)&rtos->current_thread);
	if (retval != ERROR_OK) {
		LOG_ERROR("Could not read rhino current thread from target");
		return retval;
	}

	if ((thread_list_size  == 0) || (rtos->current_thread == 0)) {
		/* Either : No RTOS threads - there is always at least the current execution though */
		/* OR     : No current thread - all threads suspended - show the current execution
		 * of idling */
		char tmp_str[] = "Current Execution";
		thread_list_size++;
		tasks_found++;
		rtos->thread_details = malloc(
				sizeof(struct thread_detail) * thread_list_size);
		rtos->thread_details->threadid = 1;
		rtos->thread_details->exists = true;
		rtos->thread_details->extra_info_str = NULL;
		rtos->thread_details->thread_name_str = malloc(sizeof(tmp_str));
		strcpy(rtos->thread_details->thread_name_str, tmp_str);

		if (thread_list_size == 1) {
			rtos->thread_count = 1;
			return ERROR_OK;
		}
	} else {
		/* create space for new thread details */
		rtos->thread_details = malloc(
				sizeof(struct thread_detail) * thread_list_size);
	}

	/* loop over all threads */
	thread_list_next = thread_list_head;

    /* Get the location of the next thread structure. */
    retval = target_read_buffer(rtos->target,
            thread_list_next,
            param->pointer_width,
            (uint8_t *) &thread_list_next);
    if (retval != ERROR_OK) {
        LOG_ERROR("Error reading next thread pointer in rhino thread list");
        return retval;
    }

	while ((thread_list_next && thread_list_next != thread_list_head) && (tasks_found < thread_list_size)) {

		#define ECOS_THREAD_NAME_STR_SIZE (200)
		char tmp_str[ECOS_THREAD_NAME_STR_SIZE];
		unsigned int i = 0;
		uint32_t name_ptr = 0;
        uint32_t thread_ptr;

        /*thread head is 36 bytes before list*/
        thread_ptr = thread_list_next - RHINO_LIST_OFFSET;

		/* Save the thread pointer */
		rtos->thread_details[tasks_found].threadid = thread_ptr;

		/* read the name pointer */
		retval = target_read_buffer(rtos->target,
				thread_ptr + param->thread_name_offset,
				param->pointer_width,
				(uint8_t *)&name_ptr);

		if (retval != ERROR_OK) {
			LOG_ERROR("Could not read rhino thread name pointer from target");
			return retval;
		}

		/* Read the thread name */
		retval =
			target_read_buffer(rtos->target,
				name_ptr,
				ECOS_THREAD_NAME_STR_SIZE,
				(uint8_t *)&tmp_str);
		if (retval != ERROR_OK) {
			LOG_ERROR("Error reading thread name from rhino target");
			return retval;
		}
		tmp_str[ECOS_THREAD_NAME_STR_SIZE-1] = '\x00';

		if (tmp_str[0] == '\x00')
			strcpy(tmp_str, "No Name");

		rtos->thread_details[tasks_found].thread_name_str =
			malloc(strlen(tmp_str)+1);
		strcpy(rtos->thread_details[tasks_found].thread_name_str, tmp_str);

		/* Read the thread status */
		int64_t thread_status = 0;
		retval = target_read_buffer(rtos->target,
				thread_ptr + param->thread_state_offset,
				1,
				(uint8_t *)&thread_status);
		if (retval != ERROR_OK) {
			LOG_ERROR("Error reading thread state from rhino target");
			return retval;
		}

		for (i = 0; (i < ECOS_NUM_STATES) && (rhino_thread_states[i].value != thread_status); i++) {
			/*
			 * empty
			 */
		}

		const char *state_desc;
		if  (i < ECOS_NUM_STATES)
			state_desc = rhino_thread_states[i].desc;
		else
			state_desc = "Unknown state";

		rtos->thread_details[tasks_found].extra_info_str = malloc(strlen(
					state_desc)+8);
		sprintf(rtos->thread_details[tasks_found].extra_info_str, "State: %s", state_desc);

		rtos->thread_details[tasks_found].exists = true;

		tasks_found++;

		/* Get the location of the next thread structure. */
		retval = target_read_buffer(rtos->target,
				thread_list_next,
				param->pointer_width,
				(uint8_t *) &thread_list_next);
		if (retval != ERROR_OK) {
			LOG_ERROR("Error reading next thread pointer in rhino thread list");
			return retval;
		}

	};

	rtos->thread_count = tasks_found;
	return 0;
}

static int rhino_get_thread_reg_list(struct rtos *rtos, int64_t thread_id, char **hex_reg_list)
{
    int retval;
    const struct rhino_params *param;
    int64_t  stack_ptr  = 0;

    *hex_reg_list = NULL;
    if (rtos == NULL)
        return -1;

    if (thread_id == 0)
        return -2;

    if (rtos->rtos_specific_params == NULL)
        return -1;

	param = (const struct rhino_params *) rtos->rtos_specific_params;

	/* Read the stack pointer */
	retval = target_read_buffer(rtos->target,
			thread_id + param->thread_stack_offset,
			param->pointer_width,
			(uint8_t *)&stack_ptr);
	if (retval != ERROR_OK) {
		LOG_ERROR("Error reading stack frame from ThreadX thread");
		return retval;
	}

	LOG_INFO("thread: 0x%" PRIx64 ", stack_ptr=0x%" PRIx64, (uint64_t)thread_id, (uint64_t)stack_ptr);

	if (stack_ptr == 0) {
		LOG_ERROR("null stack pointer in thread");
		return -5;
	}

	const struct rtos_register_stacking *stacking_info =
			get_stacking_info(rtos, stack_ptr);

	if (stacking_info == NULL) {
		LOG_ERROR("Unknown stacking info for thread id=0x%" PRIx64, (uint64_t)thread_id);
		return -6;
	}

	return rtos_generic_stack_read(rtos->target, stacking_info, stack_ptr, hex_reg_list);

}


static int rhino_get_symbol_list_to_lookup(symbol_table_elem_t *symbol_list[])
{
	unsigned int i;
	*symbol_list = calloc(
			ARRAY_SIZE(rhino_symbol_list), sizeof(symbol_table_elem_t));

	for (i = 0; i < ARRAY_SIZE(rhino_symbol_list); i++)
		(*symbol_list)[i].symbol_name = rhino_symbol_list[i];

	return 0;
}

static int rhino_detect_rtos(struct target *target)
{
	if ((target->rtos->symbols != NULL) &&
			(target->rtos->symbols[rhino_VAL_thread_list].address != 0)) {
		/* looks like rhino */
		return 1;
	}
	return 0;
}

static int rhino_create(struct target *target)
{
	int i = 0;
	while ((i < ECOS_NUM_PARAMS) &&
		(0 != strcmp(rhino_params_list[i].target_name, target->type->name))) {
		i++;
	}
	if (i >= ECOS_NUM_PARAMS) {
		LOG_ERROR("Could not find target in rhino compatibility list");
		return -1;
	}

	target->rtos->rtos_specific_params = (void *) &rhino_params_list[i];
	target->rtos->current_thread = 0;
    target->rtos->sys_stat       = RHINO_STOPPED;
	target->rtos->thread_details = NULL;
	return 0;
}
