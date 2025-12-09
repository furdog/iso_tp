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

/******************************************************************************
 * ISO-TP DEFINITIONS
 *****************************************************************************/
#define ISO_TP_MAX_FRAME_SIZE 8u /**< Max frame size (classic CAN2.0 atm) */

/******************************************************************************
 * ISO-TP COMMON
 *****************************************************************************/
/** CAN2.0 simplified frame structure. */
struct iso_tp_can_frame {
	uint32_t id;	  /**< Frame identifier. */
	uint8_t  len;	  /**< Data length code (0-8). */
	uint8_t  data[ISO_TP_MAX_FRAME_SIZE]; /**< Frame data payload. */
};

/******************************************************************************
 * ISO-TP CLASS
 *****************************************************************************/
/** N_PDU (Network Protocol Data Units).
 * In simple terms it just identifies ISO-TP message type. */
enum _iso_tp_n_pdu_type {
	_ISO_TP_N_PDU_TYPE_UNKNOWN, /**< N_PDU is not known */
	_ISO_TP_N_PDU_TYPE_SF, /**< N_PDU SingleFrame      (SF) */
	_ISO_TP_N_PDU_TYPE_FF, /**< N_PDU FirstFrame       (FF) */
	_ISO_TP_N_PDU_TYPE_CF, /**< N_PDU ConsecutiveFrame (CF) */
	_ISO_TP_N_PDU_TYPE_FC  /**< N_PDU FlowControl      (FC) */
};

/** Events emited by ISO-TP state machine */
enum iso_tp_event {
	ISO_TP_EVENT_NONE, /**< No event, proceed. */
	ISO_TP_EVENT_PLEASE_CONFIGURE /**< User must call iso_tp_set_config */
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
	enum _iso_tp_n_pdu_type type;   /**< N_PDU type or NAME */
	struct _iso_tp_n_pci	n_pci;  /**< N_PCI info */
	uint8_t			n_data[ISO_TP_MAX_FRAME_SIZE]; /**< Payload */
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

	self->_cfg.mode = ISO_TP_MODE_INVALID;
	self->_cfg.src  = 0x00u;
	self->_cfg.dst  = 0x00u;

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

	if ((self->_state == (uint8_t)_ISO_TP_STATE_LISTEN) &&
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


/** Deduce variation of _ISO_TP_N_PDU_TYPE_SF */
enum _iso_tp_n_pdu_type _iso_tp_deduce_n_pdu_type_sf(struct iso_tp *self,
						struct iso_tp_can_frame *f)
{
	enum _iso_tp_n_pdu_type type = _ISO_TP_N_PDU_TYPE_UNKNOWN;

	uint8_t dlc = (f.data[0] & 0x0F);

	/* Extended SF frame */
	if (dlc == 0) {
		/* Not supported */
	} else if ((dlc <= 6u) && (f->len >= dlc)) {
		type == _ISO_TP_N_PDU_TYPE_SF;
	} else {
		/* >6 byte long frames not yet supported */
	}

	return type;
}

/** Deduce N_PDU based on frame contents.
 *  TODO return more detailed information about PDU,
 *  probably return N_PCI struct */
enum _iso_tp_n_pdu_type _iso_tp_deduce_n_pdu_type_and_store_n_pci(
						struct iso_tp *self,
						struct iso_tp_can_frame *f
						struct iso_tp_n_pci *n_pci)
{
	enum _iso_tp_n_pdu_type type = _ISO_TP_N_PDU_TYPE_UNKNOWN;

	if (f->len >= 1u) {
		/* Variant of _ISO_TP_N_PDU_TYPE_SF */
		if ((f.data[0] & 0xF0) == 0x00u) {
			_iso_tp_deduce_n_pdu_type_sf(self, f);
		}

		&&
	    ((f.data[0] & 0xF0) == 0x00u) &&
	    ((f.data[0] & 0x0F) >  0u) {
		uint8_t dlc = (f.data[0] & 0x0F);
		if ((dlc <= 6u) && (f->len >= dlc)) {
			/* >6 byte long frames not supported
			 * in this mode, yet */
			type == _ISO_TP_N_PDU_TYPE_SF;
		}
	} else if ( (self->f.len >= 1u) &&
	    ((f.data[0] & 0xF0) == 0x00u) &&
	    ((f.data[0] & 0x0F) == 0u) {
		/* >8 byte long frames not supported yet */
	} else if (f->len > 2u) &&
	    ((f.data[0] & 0xF0) == 0x10u) &&
	    ((f.data[0] & 0x0F) >  0u) {
	} else {}

	return type;
}

/** Begin state transition based on current N_PDU */
void _iso_tp_switch_state_based_on_n_pdu_type(struct iso_tp *self,
					      enum _iso_tp_n_pdu_type type)
{
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
		if (self->_cfg.mode == ISO_TP_MODE_INVALID) {
			ev = ISO_TP_EVENT_PLEASE_CONFIGURE;
			break;
		}

		/* @@ PREPARE TRANSITION TO THE NEXT STATE @@ */
		self->_state = _ISO_TP_STATE_LISTEN;

		break;

	case _ISO_TP_STATE_LISTEN_N_PDU: {
		enum _iso_tp_n_pdu_type n_pdu_type;

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

		n_pdu_type = _iso_tp_deduce_n_pdu(self, &self->_rx_frame);

		if (_iso_tp_switch_state_based_on_n_pdu_type(self, n_pdu_type))
		{
		};

		break;
	}

	return ev;
}
