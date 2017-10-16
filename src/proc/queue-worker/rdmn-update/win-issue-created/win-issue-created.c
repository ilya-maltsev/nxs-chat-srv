#include <nxs-core/nxs-core.h>

// clang-format off

/* Project core include */
#include <nxs-chat-srv-core.h>
#include <nxs-chat-srv-meta.h>
#include <nxs-chat-srv-collections.h>
#include <nxs-chat-srv-units.h>

#include <proc/queue-worker/rdmn-update/win-issue-created/win-issue-created.h>
#include <proc/queue-worker/rdmn-update/win-issue-created/ctx/win-issue-created-ctx.h>
#include <proc/queue-worker/rdmn-update/win-issue-created/win-issue-created-subproc.h>

/* Definitions */



/* Project globals */
extern		nxs_process_t			process;
extern		nxs_chat_srv_cfg_t		nxs_chat_srv_cfg;

/* Module typedefs */



/* Module declarations */



/* Module internal (static) functions prototypes */

// clang-format on

// clang-format off

/* Module initializations */

static u_char		_s_private_message[]	= {NXS_CHAT_SRV_UTF8_PRIVATE_MESSAGE};

/* Module global functions */

// clang-format on

nxs_chat_srv_err_t
        nxs_chat_srv_p_queue_worker_rdmn_update_win_issue_created_runtime(nxs_chat_srv_m_rdmn_update_t *      update,
                                                                          size_t                              tlgrm_userid,
                                                                          nxs_chat_srv_u_rdmn_attachments_t * rdmn_attachments_ctx,
                                                                          nxs_chat_srv_u_tlgrm_attachments_t *tlgrm_attachments_ctx)
{
	nxs_chat_srv_u_tlgrm_sendmessage_t *tlgrm_sendmessage_ctx;
	nxs_string_t *s, message, private_issue, description_fmt, project_fmt, subject_fmt, author_fmt, status_fmt, priority_fmt,
	        assigned_to_fmt, file_name, file_path, description;
	nxs_buf_t *                       out_buf;
	nxs_array_t                       d_chunks;
	nxs_bool_t                        response_status;
	nxs_chat_srv_m_tlgrm_message_t    tlgrm_message;
	nxs_chat_srv_m_rdmn_attachment_t *rdmn_attachment;
	nxs_chat_srv_u_db_issues_t *      db_issue_ctx;
	size_t                            i, message_id;

	nxs_chat_srv_err_t rc;

	rc = NXS_CHAT_SRV_E_OK;

	nxs_string_init(&message);
	nxs_string_init_empty(&private_issue);
	nxs_string_init_empty(&description_fmt);
	nxs_string_init_empty(&project_fmt);
	nxs_string_init_empty(&subject_fmt);
	nxs_string_init_empty(&author_fmt);
	nxs_string_init_empty(&status_fmt);
	nxs_string_init_empty(&priority_fmt);
	nxs_string_init_empty(&assigned_to_fmt);
	nxs_string_init_empty(&file_name);
	nxs_string_init_empty(&file_path);
	nxs_string_init_empty(&description);

	nxs_array_init2(&d_chunks, nxs_string_t);

	tlgrm_sendmessage_ctx = nxs_chat_srv_u_tlgrm_sendmessage_init();
	db_issue_ctx          = nxs_chat_srv_u_db_issues_init();

	nxs_chat_srv_c_tlgrm_message_init(&tlgrm_message);

	if(update->data.issue.is_private == NXS_YES) {

		nxs_string_printf(&private_issue, NXS_CHAT_SRV_RDMN_MESSAGE_ISSUE_CREATED_IS_PRIVATE, _s_private_message);
	}

	nxs_chat_srv_c_tlgrm_format_escape_html(&description_fmt, &update->data.issue.description);
	nxs_chat_srv_c_tlgrm_format_escape_html(&project_fmt, &update->data.issue.project.name);
	nxs_chat_srv_c_tlgrm_format_escape_html(&subject_fmt, &update->data.issue.subject);
	nxs_chat_srv_c_tlgrm_format_escape_html(&author_fmt, &update->data.issue.author.name);
	nxs_chat_srv_c_tlgrm_format_escape_html(&status_fmt, &update->data.issue.status.name);
	nxs_chat_srv_c_tlgrm_format_escape_html(&priority_fmt, &update->data.issue.priority.name);
	nxs_chat_srv_c_tlgrm_format_escape_html(&assigned_to_fmt, &update->data.issue.assigned_to.name);

	nxs_string_printf(&message,
	                  NXS_CHAT_SRV_RDMN_MESSAGE_ISSUE_CREATED,
	                  &nxs_chat_srv_cfg.rdmn.host,
	                  update->data.issue.id,
	                  &project_fmt,
	                  update->data.issue.id,
	                  &subject_fmt,
	                  &private_issue,
	                  &author_fmt,
	                  &status_fmt,
	                  &priority_fmt,
	                  &assigned_to_fmt);

	nxs_chat_srv_c_tlgrm_make_message_chunks(&message, &description_fmt, &d_chunks);

	/* create new comment: send description chunks */
	for(i = 0; i < nxs_array_count(&d_chunks); i++) {

		s = nxs_array_get(&d_chunks, i);

		nxs_chat_srv_u_tlgrm_sendmessage_add(tlgrm_sendmessage_ctx, tlgrm_userid, s, NXS_CHAT_SRV_M_TLGRM_PARSE_MODE_TYPE_HTML);

		nxs_chat_srv_u_tlgrm_sendmessage_disable_web_page_preview(tlgrm_sendmessage_ctx);

		if(nxs_chat_srv_u_tlgrm_sendmessage_push(tlgrm_sendmessage_ctx) != NXS_CHAT_SRV_E_OK) {

			nxs_error(rc, NXS_CHAT_SRV_E_ERR, error);
		}

		out_buf = nxs_chat_srv_u_tlgrm_sendmessage_get_response_buf(tlgrm_sendmessage_ctx);

		if(nxs_chat_srv_c_tlgrm_message_result_pull_json(&tlgrm_message, &response_status, out_buf) == NXS_CHAT_SRV_E_OK) {

			nxs_chat_srv_u_db_issues_set(db_issue_ctx, tlgrm_userid, tlgrm_message.message_id, update->data.issue.id);
		}
	}

	/* sending attachments to user */

	for(i = 0; i < nxs_array_count(&update->data.issue.attachments); i++) {

		rdmn_attachment = nxs_array_get(&update->data.issue.attachments, i);

		if(nxs_chat_srv_u_rdmn_attachments_download(
		           rdmn_attachments_ctx, rdmn_attachment->id, &file_name, &file_path, &description) == NXS_CHAT_SRV_E_OK) {

			if(nxs_chat_srv_u_tlgrm_attachments_send_photo(
			           tlgrm_attachments_ctx, tlgrm_userid, &file_path, &description, &message_id) == NXS_CHAT_SRV_E_OK) {

				nxs_chat_srv_u_db_issues_set(db_issue_ctx, tlgrm_userid, message_id, update->data.issue.id);
			}
		}
	}

error:

	tlgrm_sendmessage_ctx = nxs_chat_srv_u_tlgrm_sendmessage_free(tlgrm_sendmessage_ctx);
	db_issue_ctx          = nxs_chat_srv_u_db_issues_free(db_issue_ctx);

	nxs_chat_srv_c_tlgrm_message_free(&tlgrm_message);

	for(i = 0; i < nxs_array_count(&d_chunks); i++) {

		s = nxs_array_get(&d_chunks, i);

		nxs_string_free(s);
	}

	nxs_array_free(&d_chunks);

	nxs_string_free(&message);
	nxs_string_free(&private_issue);
	nxs_string_free(&description_fmt);
	nxs_string_free(&project_fmt);
	nxs_string_free(&subject_fmt);
	nxs_string_free(&author_fmt);
	nxs_string_free(&status_fmt);
	nxs_string_free(&priority_fmt);
	nxs_string_free(&assigned_to_fmt);
	nxs_string_free(&file_name);
	nxs_string_free(&file_path);
	nxs_string_free(&description);

	return rc;
}

/* Module internal (static) functions */
