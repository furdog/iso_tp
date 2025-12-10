/**
 * @file iso_tp.h
 * @brief ISO-TP State machine implementation (Hardware-Agnostic)
 *
 * This file contains the software implementation of the ISO-TP logic.
 * The design is hardware-agnostic, requiring an external adaptation layer 
 * for hardware interaction.
 *
 * **Reference Specification:**
 * Implemented based on the technical specifications outlined in
 * ISO 15765-2. The specification is not included in this
 * repository due to legal reasons.
 *
 * **Conventions:**
 * C89, Linux kernel style, MISRA, rule of 10, No hardware specific code,
 * only generic C and some binding layer. Be extra specific about types.
 * 
 * Scientific units where posible at end of the names, for example:
 * - timer_10s (timer_10s has a resolution of 10s per bit)
 * - power_150w (power 150W per bit or 0.15kw per bit)
 *
 * Keep variables without units if they're unknown or not specified or hard
 * to define with short notation.
 * 
 * ```LICENSE
 * Copyright (c) 2025 furdog <https://github.com/furdog>
 *
 * SPDX-License-Identifier: 0BSD
 * ```
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h> /* for memcpy */

/******************************************************************************
 * ISO-TP DEFINITIONS
 *****************************************************************************/
#define ISO_TP_MAX_CAN_DL 8u /**< Maximum CAN dlc allowed */

/******************************************************************************
 * ISO-TP COMMON
 *****************************************************************************/
/** CAN2.0 (or FD) simplified frame structure. */
struct iso_tp_can_frame {
	uint32_t id;	  /**< Frame identifier. */
	uint8_t  len;	  /**< Data length code (0-8) (0-64 for canfd). */
	uint8_t  data[ISO_TP_MAX_CAN_DL]; /**< Frame data payload. */
};

/******************************************************************************
 * ISO-TP CLASS
 *****************************************************************************/
/** N_PDU (Network Protocol Data Units).
 * In simple terms it just identifies ISO-TP message type. */
enum _iso_tp_n_pdu_type {
	_ISO_TP_N_PDU_TYPE_UNKNOWN, /**< N_PDU is not known */
	_ISO_TP_N_PDU_TYPE_INVALID, /**< N_PDU is not valid */
	_ISO_TP_N_PDU_TYPE_SF, /**< N_PDU SingleFrame      (SF) */
	_ISO_TP_N_PDU_TYPE_FF, /**< N_PDU FirstFrame       (FF) */
	_ISO_TP_N_PDU_TYPE_CF, /**< N_PDU ConsecutiveFrame (CF) */
	_ISO_TP_N_PDU_TYPE_FC  /**< N_PDU FlowControl      (FC) */
};

/** Events emited by ISO-TP state machine */
enum iso_tp_event {
	ISO_TP_EVENT_NONE, /**< No event, proceed */
	ISO_TP_EVENT_INVALID_CONFIG, /**< Providen config is invalid */
	ISO_TP_EVENT_N_PDU /**< N_PDU detected */
};

/** Internal FSM state */
enum _iso_tp_state
{
	_ISO_TP_STATE_CONFIG, /**< Wait user to configure */
	_ISO_TP_STATE_LISTEN_N_PDU  /**< Listen for first N_PDU message */
};

/** Working modes. Only bridge mode is supported yet. */
enum iso_tp_mode {
	ISO_TP_MODE_INVALID,

	/** Bridge means that no communication is initiated by ISO-TP,
	  * but instead it works like a bridge or filter: src -> bridge -> dst
	  * Can be used in applications that require message filtering.
	  * @note this is not standard feature of ISO-TP */
	ISO_TP_MODE_BRIDGE
};

/** Structure to configure main instance.
 *  src and dst are used for connection between two endpoints.
 *  The correct order of src and dst does not matter for P2P endpoints,
 *  but it's recomended to set src for master and dst for slave devices
 *  outside of P2P applications.
 *  @attention P2P communication is not yet implemented. Though ISO-TP is
 *  originally a P2P protocol. Currently it's `src` centered, which is suitable
 *  for master/slave applications like diagnostic through OBD II */
struct iso_tp_config {
	uint8_t mode; /**< Configure operation mode. @note Not standard */

	uint32_t src; /**< Configure source      endpoint address */
	uint32_t dst; /**< Configure destination endpoint address */

	uint8_t tx_dl; /**< Max DLC for TX limited by ISO_TP_MAX_CAN_DL. */
	uint8_t rx_dl; /**< Max DLC for TX limited by ISO_TP_MAX_CAN_DL.
			Will be deduced automatically,
			so no configuration needed. */

	uint8_t min_ff_dl; /**< Min DLC for FF frame.
			    Does not require configuration */
};

/** N_PCI (Network Protocol Control Information).
 * In simple terms it just stores information for specific N_PDU type. */
struct _iso_tp_n_pci
{
	uint32_t dlc; /**< Message DLC */

	uint8_t sn; /**< SequenceNumber */

	uint8_t fs; /**< FlowStatus */

	uint8_t bs; /**< BlockSize */

	uint8_t min_st; /**< SeparationTime minimum */
};

/** N_PDU (Network Protocol Data Unit) */
struct _iso_tp_n_pdu
{
	uint8_t			type;  /**< N_PDU type or NAME */
	struct _iso_tp_n_pci	n_pci; /**< N_PCI info */
	uint8_t			n_data[ISO_TP_MAX_CAN_DL]; /**< Payload */

	/* bool extdlc; */ /**< Tells if N_PDU dlc is extended
				@note Not standard */
};

/** Main instance */
struct iso_tp {
	uint8_t _state;

	struct iso_tp_config _cfg;

	/* Intermediate */
	bool _has_tx; /**< Active if TX frame is available for send */
	bool _has_rx; /**< Active if RX frame is available for receiving */

	struct iso_tp_can_frame _tx_frame; /**< Frame to transmit */
	struct iso_tp_can_frame _rx_frame; /**< Received frame */

	uint8_t *_buf; /**< Buffer to store long messages */
	size_t   _len; /**< Buffer length */
};

/** Init main instance. User defined buffer and buffer len must be passed. */
void iso_tp_init(struct iso_tp *self, uint8_t *buf, size_t len)
{
	self->_state = _ISO_TP_STATE_CONFIG;

	self->_cfg.mode  = ISO_TP_MODE_INVALID;
	self->_cfg.src   = 0x00u;
	self->_cfg.dst   = 0x00u;
	self->_cfg.tx_dl = 8u; /** Assume CAN2.0 by default */
	self->_cfg.rx_dl = 8u; /** Assume CAN2.0 by default */
	self->_cfg.min_ff_dl = 8u;

	self->_has_tx = false;
	self->_has_rx = false;

	/*self->_src_sv_frame = ??;*/
	/*self->_dst_sv_frame = ??;*/

	self->_buf = buf;
	self->_len = len;
}

/** Configures main instance. Call this method after init. */
void iso_tp_set_config(struct iso_tp *self, struct iso_tp_config *cfg)
{
	/* Only set config in this specific state */
	if (self->_state == (uint8_t)_ISO_TP_STATE_CONFIG) {
		self->_cfg = *cfg;
	}
}

/** Push RX CAN frame for processing, returns false if busy. */
bool iso_tp_push_frame(struct iso_tp *self, struct iso_tp_can_frame *f)
{
	bool result = false;

	if ((self->_state == (uint8_t)_ISO_TP_STATE_LISTEN_N_PDU) &&
	    !self->_has_rx) {
		self->_has_rx = true;

		self->_rx_frame = *f;

		result = true;
	}

	return result;
}

/** Pop TX CAN frame, returns false if busy. */
bool iso_tp_pop_frame(struct iso_tp *self, struct iso_tp_can_frame *f)
{
	bool result = false;

	if (self->_has_tx) {
		self->_has_tx = false;

		*f = self->_tx_frame;

		result = true;
	}

	return result;
}


/** Deduce variation of _ISO_TP_N_PDU_TYPE_SF
 *  Currently only Normal addressing is used TODO */
void _iso_tp_deduce_n_pdu_type_sf(struct iso_tp *self,
				  struct _iso_tp_n_pdu *n_pdu,
				  struct iso_tp_can_frame *f)
{
	uint8_t dlc = (f->data[0] & 0x0Fu);

	(void)self;

	/* Invalid(0u) DLC means extended frame */
	if (dlc == 0u) {
		/* Extended frame NOT YET supported
		 * (Do not confuse with extended addressing) */
	} else if (f->len < (dlc + 1u)) {
		/* CAN frame DLC should not be shorter than N_PDU */
	} else if (dlc <= 7u) {
		n_pdu->type = _ISO_TP_N_PDU_TYPE_SF;

		/* Copy data to N_PDU */
		(void)memcpy(n_pdu->n_data, &f->data[1], dlc);
	} else {}
}

/** Deduce variation of _ISO_TP_N_PDU_TYPE_FF
 *  Currently only Normal addressing is used TODO */
void _iso_tp_deduce_n_pdu_type_ff(struct iso_tp *self,
				  struct _iso_tp_n_pdu *n_pdu,
				  struct iso_tp_can_frame *f)
{
	uint32_t dlc = ((f->data[0] & 0x0Fu) << 8u) | f->data[1];

	uint8_t ff_dl = 0u;

	/* Setup rx_dl based on received CAN DL */
	/* TODO See: Table 7 — Received CAN_DL to RX_DL mapping table */
	self->_cfg.rx_dl = f->len;

	/* Calculate FF_DL based on RX_DL and FF_DL(min) */
	ff_dl = (self->_cfg.rx_dl > self->_cfg.min_ff_dl) ?
			self->_cfg.rx_dl : self->_cfg.min_ff_dl;

	/* Invalid(0u) DLC means extended frame */
	if (dlc == 0u) {
		/* Extended frame NOT YET supported
		 * (Do not confuse with extended addressing) */
	} else if (dlc >= ff_dl) {
		n_pdu->type = _ISO_TP_N_PDU_TYPE_FF;

		/* Copy data to N_PDU */
		(void)memcpy(n_pdu->n_data, &f->data[2], ff_dl - 2u);
	} else {}
}

/** Deduce N_PDU based on frame contents.
 * Based on: ISO 15765-2:2016(E) Table 9 — Summary of N_PCI bytes */
void _iso_tp_deduce_n_pdu(struct iso_tp *self, struct _iso_tp_n_pdu *n_pdu,
			  struct iso_tp_can_frame *f)
{
	n_pdu->type = _ISO_TP_N_PDU_TYPE_INVALID;

	/* _ISO_TP_N_PDU_TYPE_SF */
	if ((f->len >= 1u) && ((f->data[0] & 0xF0u) == 0x00u)) {
		_iso_tp_deduce_n_pdu_type_sf(self, n_pdu, f);

	/* _ISO_TP_N_PDU_TYPE_CF */ 
	} else if ((f->len >= 1u) && ((f->data[0] & 0xF0u) == 0x20u)) {
		/* It's a consecutive frame! */
		n_pdu->type = _ISO_TP_N_PDU_TYPE_CF;
		n_pdu->n_pci.sn = (f->data[0] & 0x0Fu);

		/* Copy data to N_PDU */
		(void)memcpy(n_pdu->n_data, &f->data[1], f->len - 1u);

	/* _ISO_TP_N_PDU_TYPE_FF */ 
	} else if ((f->len >= 2u) && ((f->data[0] & 0xF0u) == 0x10u)) {
		_iso_tp_deduce_n_pdu_type_ff(self, n_pdu, f);

	/* _ISO_TP_N_PDU_TYPE_FC */
	} else if ((f->len >= 3u) && ((f->data[0] & 0xF0u) == 0x30u)) {
		/* Simplest case, we don't assume a shit */
		n_pdu->type = _ISO_TP_N_PDU_TYPE_FC;
		n_pdu->n_pci.fs     = (f->data[0] & 0x0Fu);
		n_pdu->n_pci.bs     = f->data[1];
		n_pdu->n_pci.min_st = f->data[2];
	} else {}
}

/** Main instance state machine. Works step by step. Returns events during
 *  operation. Must be run inside main loop. */
enum iso_tp_event iso_tp_step(struct iso_tp *self, uint32_t delta_time_ms)
{
	enum iso_tp_event ev = ISO_TP_EVENT_NONE;

	/* bool passthrough = false; */ /* Passthrough received messages */

	(void)self;
	(void)delta_time_ms;
	switch (self->_state) {
	case _ISO_TP_STATE_CONFIG:
		/* Mode and MTU must be set correctly */
		if ((self->_cfg.mode == (uint8_t)ISO_TP_MODE_INVALID) ||
		    (self->_cfg.tx_dl < 8u)) {
			ev = ISO_TP_EVENT_INVALID_CONFIG;
			break;
		}

		/* Min DLC min_ff_dl (see Table 14)
		 * TODO addressing types
		 * Only normal addressing mode is supported yet */
		if (self->_cfg.tx_dl == 8u) {
			self->_cfg.min_ff_dl = 8u;
		} else if (self->_cfg.tx_dl > 8u) {
			self->_cfg.min_ff_dl = self->_cfg.tx_dl - 1u;
		} else {}

		/* @@ PREPARE TRANSITION TO THE NEXT STATE @@ */
		self->_state = _ISO_TP_STATE_LISTEN_N_PDU;

		break;

	case _ISO_TP_STATE_LISTEN_N_PDU: {
		struct _iso_tp_n_pdu n_pdu;

		if (!self->_has_rx) {
			break;
		}

		/* Only listen for src messages YET */
		if (self->_rx_frame.id != self->_cfg.src) {
			break;
		}

		/* Validate frame src address */
		if (self->_rx_frame.id != self->_cfg.src) {
			break;
		}

		_iso_tp_deduce_n_pdu(self, &n_pdu, &self->_rx_frame);

		if (n_pdu.type != (uint8_t)_ISO_TP_N_PDU_TYPE_UNKNOWN) {
			ev = ISO_TP_EVENT_N_PDU;
		}

		break;
	}

	default:
		break;

	}

	return ev;
}
