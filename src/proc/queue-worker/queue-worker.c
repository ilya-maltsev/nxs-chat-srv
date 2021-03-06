#include <nxs-core/nxs-core.h>

// clang-format off

/* Project core include */
#include <nxs-chat-srv-core.h>
#include <nxs-chat-srv-meta.h>
#include <nxs-chat-srv-collections.h>
#include <nxs-chat-srv-units.h>

#include <proc/queue-worker/queue-worker.h>
#include <proc/queue-worker/ctx/queue-worker-ctx.h>
#include <proc/queue-worker/queue-worker-subproc.h>

/* Definitions */



/* Project globals */
extern		nxs_process_t			process;
extern		nxs_chat_srv_cfg_t		nxs_chat_srv_cfg;

/* Module typedefs */



/* Module declarations */

typedef struct
{
	nxs_chat_srv_m_ra_queue_type_t		type;
	nxs_chat_srv_err_t			(*handler)(nxs_chat_srv_m_ra_queue_type_t type, nxs_string_t *data, void *p_meta);
	void					*p_meta;
} nxs_chat_srv_p_queue_worker_handler_type_t;

/* Module internal (static) functions prototypes */

// clang-format on

static nxs_chat_srv_err_t nxs_chat_srv_p_queue_worker_process(nxs_chat_srv_p_queue_worker_ctx_t *p_ctx, nxs_array_t *handlers_type);

static void nxs_chat_srv_p_queue_worker_handlers_type_init(nxs_array_t *handlers_type);
static void nxs_chat_srv_p_queue_worker_handlers_type_free(nxs_array_t *handlers_type);
static void nxs_chat_srv_p_queue_worker_handlers_type_add(nxs_array_t *                  handlers_type,
                                                          nxs_chat_srv_m_ra_queue_type_t type,
                                                          nxs_chat_srv_err_t (*handler)(nxs_chat_srv_m_ra_queue_type_t type,
                                                                                        nxs_string_t *                 data,
                                                                                        void *                         p_meta),
                                                          void *p_meta);

static void nxs_chat_srv_p_queue_worker_sighandler_term(int sig, void *data);
static void nxs_chat_srv_p_queue_worker_sighandler_usr1(int sig, void *data);

// clang-format off

/* Module initializations */



/* Module global functions */

// clang-format on

nxs_chat_srv_err_t nxs_chat_srv_p_queue_worker_runtime(void)
{
	nxs_chat_srv_p_queue_worker_ctx_t p_ctx;
	nxs_chat_srv_err_t                rc, ec;
	nxs_array_t                       handlers_type;

	if(nxs_chat_srv_p_queue_worker_ctx_init(&p_ctx) != NXS_CHAT_SRV_E_OK) {

		nxs_error(rc, NXS_CHAT_SRV_E_ERR, error);
	}

	rc = NXS_CHAT_SRV_E_OK;

	nxs_chat_srv_p_queue_worker_handlers_type_init(&handlers_type);

	nxs_proc_signal_set(
	        &process, SIGTERM, NXS_PROCESS_SA_FLAG_EMPTY, &nxs_chat_srv_p_queue_worker_sighandler_term, &p_ctx, NXS_PROCESS_FORCE_NO);
	nxs_proc_signal_set(
	        &process, SIGUSR1, NXS_PROCESS_SA_FLAG_EMPTY, &nxs_chat_srv_p_queue_worker_sighandler_usr1, &p_ctx, NXS_PROCESS_FORCE_NO);

	nxs_proc_signal_set(&process, SIGINT, NXS_PROCESS_SA_FLAG_EMPTY, NXS_SIG_IGN, NULL, NXS_PROCESS_FORCE_NO);

	// clang-format off

	nxs_chat_srv_p_queue_worker_handlers_type_add(&handlers_type,	NXS_CHAT_SRV_M_RA_QUEUE_TYPE_TLGRM_UPDATE,	&nxs_chat_srv_p_queue_worker_tlgrm_update_add,		NULL);
	nxs_chat_srv_p_queue_worker_handlers_type_add(&handlers_type,	NXS_CHAT_SRV_M_RA_QUEUE_TYPE_RDMN_UPDATE,	&nxs_chat_srv_p_queue_worker_rdmn_update_runtime,	&p_ctx.rdmn_update_ctx);

	// clang-format on

	while(1) {

		nxs_proc_signal_block(&process, SIGTERM, SIGUSR1, NXS_PROCESS_SIG_END_ARGS);

		while((ec = nxs_chat_srv_p_queue_worker_process(&p_ctx, &handlers_type)) == NXS_CHAT_SRV_E_OK) {

			/* continue */
		};

		if(ec != NXS_CHAT_SRV_E_TIMEOUT) {

			nxs_error(rc, NXS_CHAT_SRV_E_ERR, error);
		}

		usleep(nxs_chat_srv_cfg.ra_queue.pop_timeout_ms);

		/* processing received signals */
		nxs_proc_signal_unblock(&process, SIGUSR1, SIGTERM, NXS_PROCESS_SIG_END_ARGS);
		nxs_proc_signal_block(&process, SIGTERM, SIGUSR1, NXS_PROCESS_SIG_END_ARGS);

		nxs_chat_srv_p_queue_worker_tlgrm_update_runtime_queue();

		nxs_proc_signal_unblock(&process, SIGUSR1, SIGTERM, NXS_PROCESS_SIG_END_ARGS);
	}

error:

	nxs_chat_srv_p_queue_worker_ctx_free(&p_ctx);

	nxs_chat_srv_p_queue_worker_handlers_type_free(&handlers_type);

	return rc;
}

/* Module internal (static) functions */

static nxs_chat_srv_err_t nxs_chat_srv_p_queue_worker_process(nxs_chat_srv_p_queue_worker_ctx_t *p_ctx, nxs_array_t *handlers_type)
{
	nxs_string_t                                data;
	nxs_chat_srv_m_ra_queue_type_t              type;
	nxs_chat_srv_p_queue_worker_handler_type_t *h;
	nxs_chat_srv_err_t                          rc;
	size_t                                      i;

	if(p_ctx == NULL) {

		return NXS_CHAT_SRV_E_PTR;
	}

	rc = NXS_CHAT_SRV_E_OK;

	nxs_string_init(&data);

	switch(nxs_chat_srv_u_ra_queue_get(nxs_chat_srv_p_queue_worker_ctx_get_ra_queue(p_ctx), &type, &data)) {

		case NXS_CHAT_SRV_E_OK:

			break;

		case NXS_CHAT_SRV_E_EXIST:

			nxs_error(rc, NXS_CHAT_SRV_E_TIMEOUT, error);

		default:

			nxs_error(rc, NXS_CHAT_SRV_E_ERR, error);
	}

	/* request processing */

	for(i = 0; i < nxs_array_count(handlers_type); i++) {

		h = nxs_array_get(handlers_type, i);

		if(h->type == type) {

			h->handler(type, &data, h->p_meta);

			nxs_error(rc, NXS_CHAT_SRV_E_OK, error);
		}
	}

	nxs_log_write_warn(&process, "[%s]: unknown rest-api queue type (type: %d)", nxs_proc_get_name(&process), type);

error:

	nxs_string_free(&data);

	return rc;
}

static void nxs_chat_srv_p_queue_worker_handlers_type_init(nxs_array_t *handlers_type)
{

	if(handlers_type == NULL) {

		return;
	}

	nxs_array_init2(handlers_type, nxs_chat_srv_p_queue_worker_handler_type_t);
}

static void nxs_chat_srv_p_queue_worker_handlers_type_free(nxs_array_t *handlers_type)
{

	if(handlers_type == NULL) {

		return;
	}

	nxs_array_free(handlers_type);
}

static void nxs_chat_srv_p_queue_worker_handlers_type_add(nxs_array_t *                  handlers_type,
                                                          nxs_chat_srv_m_ra_queue_type_t type,
                                                          nxs_chat_srv_err_t (*handler)(nxs_chat_srv_m_ra_queue_type_t type,
                                                                                        nxs_string_t *                 data,
                                                                                        void *                         p_meta),
                                                          void *p_meta)
{
	nxs_chat_srv_p_queue_worker_handler_type_t *el;

	if(handlers_type == NULL) {

		return;
	}

	el = nxs_array_add(handlers_type);

	el->type    = type;
	el->handler = handler;
	el->p_meta  = p_meta;
}

static void nxs_chat_srv_p_queue_worker_sighandler_term(int sig, void *data)
{
	nxs_chat_srv_p_queue_worker_ctx_t *p_ctx = data;

	nxs_log_write_debug(&process, "[%s]: got TERM, terminating process", nxs_proc_get_name(&process));

	nxs_chat_srv_p_queue_worker_ctx_free(p_ctx);

	exit(EXIT_SUCCESS);
}

static void nxs_chat_srv_p_queue_worker_sighandler_usr1(int sig, void *data)
{

	nxs_log_write_debug(&process, "[%s]: got SIGUSR1, going to reopen log-file", nxs_proc_get_name(&process));

	nxs_log_reopen(&process);
}
