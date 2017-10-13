#ifndef _INCLUDE_NXS_CHAT_SRV_U_TLGRM_ATTACHMENTS_H
#define _INCLUDE_NXS_CHAT_SRV_U_TLGRM_ATTACHMENTS_H

// clang-format off

/* Structs declarations */

struct nxs_chat_srv_u_tlgrm_attachments_s;

/* Prototypes */

nxs_chat_srv_u_tlgrm_attachments_t		*nxs_chat_srv_u_tlgrm_attachments_init			(void);
nxs_chat_srv_u_tlgrm_attachments_t		*nxs_chat_srv_u_tlgrm_attachments_free			(nxs_chat_srv_u_tlgrm_attachments_t *u_ctx);

nxs_chat_srv_err_t				nxs_chat_srv_u_tlgrm_attachments_download		(nxs_chat_srv_u_tlgrm_attachments_t *u_ctx, size_t tlgrm_userid, nxs_string_t *file_id, nxs_string_t *file_name, nxs_string_t *file_path);

#endif /* _INCLUDE_NXS_CHAT_SRV_U_TLGRM_ATTACHMENTS_H */