/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: Implement various notification handlers which are accessed
 * internally in pkt_capture component only.
 */

#include "wlan_pkt_capture_main.h"
#include "cfg_ucfg_api.h"
#include "wlan_pkt_capture_mon_thread.h"

enum pkt_capture_mode pkt_capture_get_mode(struct wlan_objmgr_psoc *psoc)
{
	struct pkt_psoc_priv *psoc_priv;

	if (!psoc) {
		pkt_capture_err("psoc is NULL");
		return PACKET_CAPTURE_MODE_DISABLE;
	}

	psoc_priv = pkt_capture_psoc_get_priv(psoc);
	if (!psoc_priv) {
		pkt_capture_err("psoc_priv is NULL");
		return PACKET_CAPTURE_MODE_DISABLE;
	}

	return psoc_priv->cfg_param.pkt_capture_mode;
}

QDF_STATUS
pkt_capture_register_callbacks(struct wlan_objmgr_vdev *vdev,
			       QDF_STATUS (*mon_cb)(void *, qdf_nbuf_t),
			       void *context)
{
	struct pkt_capture_vdev_priv *vdev_priv;

	if (!vdev) {
		pkt_capture_err("vdev is NULL");
		return QDF_STATUS_E_INVAL;
	}

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	if (!vdev_priv) {
		pkt_capture_err("vdev priv is NULL");
		return QDF_STATUS_E_INVAL;
	}

	vdev_priv->cb_ctx->mon_cb = mon_cb;
	vdev_priv->cb_ctx->mon_ctx = context;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS pkt_capture_deregister_callbacks(struct wlan_objmgr_vdev *vdev)
{
	struct pkt_capture_vdev_priv *vdev_priv;

	if (!vdev) {
		pkt_capture_err("vdev is NULL");
		return QDF_STATUS_E_INVAL;
	}

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	if (!vdev_priv) {
		pkt_capture_err("vdev priv is NULL");
		return QDF_STATUS_E_INVAL;
	}

	vdev_priv->cb_ctx->mon_cb = NULL;
	vdev_priv->cb_ctx->mon_ctx = NULL;

	return QDF_STATUS_SUCCESS;
}

void pkt_capture_set_pktcap_mode(struct wlan_objmgr_psoc *psoc,
				 enum pkt_capture_mode mode)
{
	struct pkt_capture_vdev_priv *vdev_priv;
	struct wlan_objmgr_vdev *vdev;

	if (!psoc) {
		pkt_capture_err("psoc is NULL");
		return;
	}

	vdev = wlan_objmgr_get_vdev_by_opmode_from_psoc(psoc,
							QDF_STA_MODE,
							WLAN_PKT_CAPTURE_ID);
	if (!vdev) {
		pkt_capture_err("vdev is NULL");
		return;
	}

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	if (vdev_priv)
		vdev_priv->cb_ctx->pkt_capture_mode = mode;
	else
		pkt_capture_err("vdev_priv is NULL");

	wlan_objmgr_vdev_release_ref(vdev, WLAN_PKT_CAPTURE_ID);
}

enum pkt_capture_mode
pkt_capture_get_pktcap_mode(struct wlan_objmgr_psoc *psoc)
{
	enum pkt_capture_mode mode = PACKET_CAPTURE_MODE_DISABLE;
	struct pkt_capture_vdev_priv *vdev_priv;
	struct wlan_objmgr_vdev *vdev;

	if (!psoc) {
		pkt_capture_err("psoc is NULL");
		return 0;
	}

	vdev = wlan_objmgr_get_vdev_by_opmode_from_psoc(psoc,
							QDF_STA_MODE,
							WLAN_PKT_CAPTURE_ID);
	if (!vdev) {
		pkt_capture_err("vdev is NULL");
		return 0;
	}

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	if (!vdev_priv)
		pkt_capture_err("vdev_priv is NULL");
	else
		mode = vdev_priv->cb_ctx->pkt_capture_mode;

	wlan_objmgr_vdev_release_ref(vdev, WLAN_PKT_CAPTURE_ID);
	return mode;
}

/**
 * pkt_capture_callback_ctx_create() - Create packet capture callback context
 * @vdev_priv: pointer to packet capture vdev priv obj
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
pkt_capture_callback_ctx_create(struct pkt_capture_vdev_priv *vdev_priv)
{
	struct pkt_capture_cb_context *cb_ctx;

	cb_ctx = qdf_mem_malloc(sizeof(*cb_ctx));
	if (!cb_ctx) {
		pkt_capture_err("MON context create failed");
		return QDF_STATUS_E_NOMEM;
	}

	vdev_priv->cb_ctx = cb_ctx;

	return QDF_STATUS_SUCCESS;
}

/**
 * pkt_capture_callback_ctx_destroy() - Destroy packet capture callback context
 * @vdev_priv: pointer to packet capture vdev priv obj
 *
 * Return: None
 */
static void
pkt_capture_callback_ctx_destroy(struct pkt_capture_vdev_priv *vdev_priv)
{
	qdf_mem_free(vdev_priv->cb_ctx);
}

/**
 * pkt_capture_mon_context_create() - Create packet capture mon context
 * @vdev_priv: pointer to packet capture vdev priv obj
 *
 * This function allocates memory for packet capture mon context
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
pkt_capture_mon_context_create(struct pkt_capture_vdev_priv *vdev_priv)
{
	struct pkt_capture_mon_context *mon_context;

	mon_context = qdf_mem_malloc(sizeof(*mon_context));
	if (!mon_context) {
		pkt_capture_err("MON context create failed");
		return QDF_STATUS_E_NOMEM;
	}

	vdev_priv->mon_ctx = mon_context;

	return QDF_STATUS_SUCCESS;
}

/**
 * pkt_capture_mon_context_destroy() - Destroy packet capture mon context
 * @vdev_priv: pointer to packet capture vdev priv obj
 *
 * Free packet capture mon context
 *
 * Return: None
 */
static void
pkt_capture_mon_context_destroy(struct pkt_capture_vdev_priv *vdev_priv)
{
	qdf_mem_free(vdev_priv->mon_ctx);
}

/**
 * pkt_capture_cfg_init() - Initialize packet capture cfg ini params
 * @psoc_priv: psoc private object
 *
 * Return: None
 */
static void
pkt_capture_cfg_init(struct pkt_psoc_priv *psoc_priv)
{
	struct pkt_capture_cfg *cfg_param;

	cfg_param = &psoc_priv->cfg_param;

	cfg_param->pkt_capture_mode = cfg_get(psoc_priv->psoc,
					      CFG_PKT_CAPTURE_MODE);
}

QDF_STATUS
pkt_capture_vdev_create_notification(struct wlan_objmgr_vdev *vdev, void *arg)
{
	struct pkt_capture_mon_context *mon_ctx;
	struct pkt_capture_vdev_priv *vdev_priv;
	QDF_STATUS status;

	vdev_priv = qdf_mem_malloc(sizeof(*vdev_priv));
	if (!vdev_priv)
		return QDF_STATUS_E_NOMEM;

	status = wlan_objmgr_vdev_component_obj_attach(
					vdev,
					WLAN_UMAC_COMP_PKT_CAPTURE,
					vdev_priv, QDF_STATUS_SUCCESS);
	if (QDF_IS_STATUS_ERROR(status)) {
		pkt_capture_err("Failed to attach vdev component obj");
		goto free_vdev_priv;
	}

	vdev_priv->vdev = vdev;

	status = pkt_capture_callback_ctx_create(vdev_priv);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		pkt_capture_err("Failed to create callback context");
		goto detach_vdev_priv;
	}

	status = pkt_capture_mon_context_create(vdev_priv);
	if (QDF_IS_STATUS_ERROR(status)) {
		pkt_capture_err("Failed to create mon context");
		goto destroy_pkt_capture_cb_context;
	}

	mon_ctx = vdev_priv->mon_ctx;

	status = pkt_capture_alloc_mon_thread(mon_ctx);
	if (QDF_IS_STATUS_ERROR(status)) {
		pkt_capture_err("Failed to alloc mon thread");
		goto destroy_mon_context;
	}

	status = pkt_capture_open_mon_thread(mon_ctx);
	if (QDF_IS_STATUS_ERROR(status)) {
		pkt_capture_err("Failed to open mon thread");
		goto open_mon_thread_fail;
	}

	return status;
open_mon_thread_fail:
	pkt_capture_free_mon_pkt_freeq(mon_ctx);
destroy_mon_context:
	pkt_capture_mon_context_destroy(vdev_priv);
destroy_pkt_capture_cb_context:
	pkt_capture_callback_ctx_destroy(vdev_priv);
detach_vdev_priv:
	wlan_objmgr_vdev_component_obj_detach(vdev,
					      WLAN_UMAC_COMP_PKT_CAPTURE,
					      vdev_priv);
free_vdev_priv:
	qdf_mem_free(vdev_priv);
	return status;
}

QDF_STATUS
pkt_capture_vdev_destroy_notification(struct wlan_objmgr_vdev *vdev, void *arg)
{
	struct pkt_capture_vdev_priv *vdev_priv;
	QDF_STATUS status;

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	if (!vdev_priv) {
		pkt_capture_err("vdev priv is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	status = wlan_objmgr_vdev_component_obj_detach(
					vdev,
					WLAN_UMAC_COMP_PKT_CAPTURE,
					vdev_priv);
	if (QDF_IS_STATUS_ERROR(status))
		pkt_capture_err("Failed to detach vdev component obj");

	pkt_capture_close_mon_thread(vdev_priv->mon_ctx);
	pkt_capture_mon_context_destroy(vdev_priv);
	pkt_capture_callback_ctx_destroy(vdev_priv);
	qdf_mem_free(vdev_priv);
	return status;
}

QDF_STATUS
pkt_capture_psoc_create_notification(struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct pkt_psoc_priv *psoc_priv;
	QDF_STATUS status;

	psoc_priv = qdf_mem_malloc(sizeof(*psoc_priv));
	if (!psoc_priv)
		return QDF_STATUS_E_NOMEM;

	status = wlan_objmgr_psoc_component_obj_attach(psoc,
				WLAN_UMAC_COMP_PKT_CAPTURE,
				psoc_priv, QDF_STATUS_SUCCESS);
	if (QDF_IS_STATUS_ERROR(status)) {
		pkt_capture_err("Failed to attach psoc component obj");
		goto free_psoc_priv;
	}

	psoc_priv->psoc = psoc;
	pkt_capture_cfg_init(psoc_priv);

	return status;

free_psoc_priv:
	qdf_mem_free(psoc_priv);
	return status;
}

QDF_STATUS
pkt_capture_psoc_destroy_notification(struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct pkt_psoc_priv *psoc_priv;
	QDF_STATUS status;

	psoc_priv = pkt_capture_psoc_get_priv(psoc);
	if (!psoc_priv) {
		pkt_capture_err("psoc priv is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	status = wlan_objmgr_psoc_component_obj_detach(psoc,
					WLAN_UMAC_COMP_PKT_CAPTURE,
					psoc_priv);
	if (QDF_IS_STATUS_ERROR(status)) {
		pkt_capture_err("Failed to detach psoc component obj");
		return status;
	}

	qdf_mem_free(psoc_priv);
	return status;
}
