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
 * All entities(names) from specs are explictly mapped in code and could
 * be found as is, for example N_PCItype in code is *_N_PCITYPE.
 * Name casing will be changed according to local coding conventions.
 * Names with spaces will be prefixed with underscore `_`
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
 *
 * Be free, be wise and take care of yourself!
 * With best wishes and respect, furdog
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h> /* for memcpy */

/******************************************************************************
 * ISO-TP DEFINITIONS
 *****************************************************************************/
#define ISO_TP_MAX_CAN_DL 8u /**< Maximum CAN dlc allowed
				@note Not explicitly stated in standard */

/******************************************************************************
 * ISO-TP TYPE AND DATA DEFINITIONS
 *
 * Mostly based on standard (and explicitly stated if not)
 *****************************************************************************/
/** CAN2.0 (or FD) simplified frame structure.
 * @note Not an explicit part of the standard */
struct iso_tp_can_frame {
	uint32_t id;	  /**< Frame identifier. */
	uint8_t  len;	  /**< Data length code (0-8) (0-64 for canfd). */
	uint8_t  data[ISO_TP_MAX_CAN_DL]; /**< Frame data payload. */
};

/** The communication model type. @note This model is NOT contained within
 *  messages itself and has to be preconfigured by user. See Table 4 */
enum iso_tp_n_tatype {
	/** CAN base format (CLASSICAL CAN, 11-bit) (a) Physical */
	ISO_TP_N_TATYPE_1,

	/** CAN base format (CLASSICAL CAN, 11-bit) (b) Functional */
	ISO_TP_N_TATYPE_2,

	/** CAN FD base format (CAN FD, 11-bit) (a) Physical */
	ISO_TP_N_TATYPE_3,

	/** CAN FD base format (CAN FD, 11-bit) (b) Functional */
	ISO_TP_N_TATYPE_4,

	/** CAN extended format (CLASSICAL CAN, 29-bit) (a) Physical */
	ISO_TP_N_TATYPE_5,

	/** CAN extended format (CLASSICAL CAN, 29-bit) (b) Functional */
	ISO_TP_N_TATYPE_6,

	/** CAN FD extended format (CAN FD, 29-bit) (a) Physical */
	ISO_TP_N_TATYPE_7,

	/** CAN FD extended format (CAN FD, 29-bit) (b) Functional */
	ISO_TP_N_TATYPE_8
};

/** N_PCI (Network Protocol Control Information Type).
 *  In simple terms it just identifies CAN frame type */
enum iso_tp_n_pcitype {
	ISO_TP_N_PCITYPE_UNKNOWN, /**< Type not known @note Not standard */
	ISO_TP_N_PCITYPE_INVALID, /**< Type not valid @note Not standard */

	ISO_TP_N_PCITYPE_SF, /**< SingleFrame      (SF) */
	ISO_TP_N_PCITYPE_FF, /**< FirstFrame       (FF) */
	ISO_TP_N_PCITYPE_CF, /**< ConsecutiveFrame (CF) */
	ISO_TP_N_PCITYPE_FC  /**< FlowControl      (FC) */
};

/** N_PCI (Network Protocol Control Information).
 * In simple terms it just stores general information about CAN frame */
struct iso_tp_n_pci
{
	uint8_t n_pcitype; /**< Network protocol control information type */

	/* SingleFrame (SF) */
	uint8_t	 sf_dl; /**< SingleFrame DataLength (SF_DL). */

	/* FirstFrame (FF) */
	uint32_t ff_dl; /**< FirstFrame  DataLength (FF_DL). */

	/* ConsecutiveFrame (CF) */
	uint8_t sn; /**< SequenceNumber */

	/* FlowControl (FC) */
	uint8_t fs;     /**< FlowStatus */
	uint8_t bs;     /**< BlockSize */
	uint8_t min_st; /**< SeparationTime minimum */
};

/** N_PDU (Network Protocol Data Unit) */
struct iso_tp_n_pdu
{
	struct iso_tp_n_pci n_pci; /**< N_PCI info */

	uint8_t	n_data[ISO_TP_MAX_CAN_DL]; /**< Payload */
	uint8_t	len_n_data; /**< n_data length @note Not standard */
};

/******************************************************************************
 * ISO-TP TYPE AND DATA DEFINITIONS AND IMPLEMENTATION
 *
 * Please note that most of the data structures, functions,
 * and common expressions in this section are not explicitly mentioned
 * in the standard and are solely part of the author's design.
 *****************************************************************************/
/** Events emited by ISO-TP state machine @note Not standard */
enum iso_tp_event {
	ISO_TP_EVENT_NONE, /**< No event, proceed */
	ISO_TP_EVENT_INVALID_CONFIG, /**< Providen config is invalid */
	ISO_TP_EVENT_N_PDU /**< N_PDU detected */
};

/** Internal FSM state @note Not standard */
enum _iso_tp_state
{
	_ISO_TP_STATE_CONFIG, /**< Wait user to configure */
	_ISO_TP_STATE_LISTEN_N_PDU  /**< Listen for first N_PDU message */
};

/** Working modes. Only bridge mode is supported yet. @note Not standard */
enum iso_tp_mode {
	ISO_TP_MODE_INVALID,

	/** Bridge means that no communication is initiated by ISO-TP,
	  * but instead it works like a bridge or filter: src -> bridge -> dst
	  * Can be used in applications that require message filtering. */
	ISO_TP_MODE_BRIDGE
};

/** Structure to configure main instance.
 *  src and dst are used for connection between two endpoints.
 *  The correct order of src and dst does not matter for P2P endpoints,
 *  but it's recomended to set src for master and dst for slave devices
 *  outside of P2P applications.
 *  @attention P2P communication is not yet implemented. Though ISO-TP is
 *  originally a P2P protocol. Currently it's `src` centered, which is suitable
 *  for master/slave applications like diagnostic through OBD II
 *  @note Not standard (except members)
 */
struct iso_tp_config {
	uint8_t mode; /**< Configure operation mode. @note Not standard */

	uint8_t n_tatype; /**< Network target address type */

	uint32_t src; /**< Configure source      endpoint address
			@note Not standard */
	uint32_t dst; /**< Configure destination endpoint address
			@note Not standard */

	uint8_t tx_dl; /**< Max DLC for TX limited by ISO_TP_MAX_CAN_DL */
	uint8_t rx_dl; /**< Max DLC for TX limited by ISO_TP_MAX_CAN_DL.
			Will be deduced automatically,
			so no configuration needed. */

	uint8_t min_ff_dl; /**< Minimum value of FF_DL based on the
				addressing scheme */
};

/** Main instance @note Not standard */
struct iso_tp {
	uint8_t _state;

	struct iso_tp_n_pdu _n_pdu;

	struct iso_tp_config _cfg;

	/* Intermediate */
	bool _has_tx; /**< Active if TX frame is available for send */
	bool _has_rx; /**< Active if RX frame is available for receiving */

	struct iso_tp_can_frame _can_tx_frame; /**< Frame to transmit */
	struct iso_tp_can_frame _can_rx_frame; /**< Received frame */
};

/** Init main instance */
void iso_tp_init(struct iso_tp *self)
{
	self->_state = _ISO_TP_STATE_CONFIG;

	(void)memset(&self->_n_pdu, 0u, sizeof(struct iso_tp_n_pdu));

	self->_cfg.mode      = ISO_TP_MODE_INVALID;
	self->_cfg.n_tatype  = ISO_TP_N_TATYPE_1; /* Used the most */
	self->_cfg.src       = 0x00u;
	self->_cfg.dst       = 0x00u;
	self->_cfg.tx_dl     = 8u; /* Assume CAN2.0 by default */
	self->_cfg.rx_dl     = 8u; /* Assume CAN2.0 by default */
	self->_cfg.min_ff_dl = 8u; /* Assume CAN2.0 by default */

	self->_has_tx = false;
	self->_has_rx = false;

	/*self->_src_sv_frame = ??;*/
	/*self->_dst_sv_frame = ??;*/
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

		self->_can_rx_frame = *f;

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

		*f = self->_can_tx_frame;

		result = true;
	}

	return result;
}

/** Deduce variation of ISO_TP_N_PCITYPE_SF.
 *  Currently only Normal addressing is used TODO */
void _iso_tp_deduce_n_pcitype_n_sf(struct iso_tp *self,
				   struct iso_tp_can_frame *f)
{
	struct iso_tp_n_pdu *n_pdu    = &self->_n_pdu;
	struct iso_tp_n_pci *n_pci    = &self->_n_pdu.n_pci;
	uint8_t		     can_dl   = f->len;
	uint8_t		    *can_data = f->data;

	/* N_PCI offset within PDU */
	/* uint8_t offset_n_pci = 0u; */

	n_pci->sf_dl = (can_data[0] & 0x0Fu);

	if (can_dl < 1u) {
		/* CAN DLC can't be less than len(N_PCI) */
	} else if (n_pci->sf_dl == 0u) {
		/* Extended frame NOT YET supported
		 * (Do not confuse with extended addressing) */
	} else if (n_pci->sf_dl > 7u) {
		/* SF_DL = 7 is only allowed with normal addressing. */
	} else if (can_dl < (1u + n_pci->sf_dl)) {
		/* CAN DLC can't be less than len(N_PCI) + N_Data */
	} else {
		/* Valid frame */
		n_pci->n_pcitype = ISO_TP_N_PCITYPE_SF;

		/* Copy data to N_PDU */
		(void)memcpy(n_pdu->n_data, &can_data[1], n_pci->sf_dl);
	}
}

/** Deduce variation of ISO_TP_N_PCITYPE_FF.
 *  Currently only Normal addressing is used TODO */
void _iso_tp_deduce_n_pcitype_n_ff(struct iso_tp *self,
				   struct iso_tp_can_frame *f)
{
	/* Commonly used */
	struct iso_tp_n_pdu *n_pdu    = &self->_n_pdu;
	struct iso_tp_n_pci *n_pci    = &self->_n_pdu.n_pci;
	uint8_t		     can_dl   = f->len;
	uint8_t		    *can_data = f->data;

	n_pci->ff_dl = ((can_data[0] & 0x0Fu) << 8u) | can_data[1];

	/* Setup rx_dl based on received CAN DL */
	/* See: Table 7 — Received CAN_DL to RX_DL mapping table.
	 * TODO implement correct RX_DL mapping */
	self->_cfg.rx_dl = can_dl;

	if (can_dl < 2u) {
		/* CAN DLC can't be less than len(N_PCI) */
	} else if (n_pci->ff_dl == 0u) {
		/* Extended frame NOT YET supported
		 * (Do not confuse with extended addressing) */
	} else if (n_pci->ff_dl < self->_cfg.min_ff_dl) {
		/* FF_DL can't be less than preconfigured min(FF_DL) */
	} else if (n_pci->ff_dl < (self->_cfg.rx_dl - 2u)) {
		/* FF_DL can't be less than RX_DL - len(N_PCI) */
	} else {
		/* Valid frame */
		n_pci->n_pcitype = ISO_TP_N_PCITYPE_FF;

		/* Copy data to N_PDU */
		(void)memcpy(n_pdu->n_data, &can_data[2], can_dl - 2u);
	}
}

/** Decode N_PDU and N_PCItype based on frame contents.
 * Based on: ISO 15765-2:2016(E) Table 9 — Summary of N_PCI bytes.
 * Currently only Normal addressing is used TODO */
void _iso_tp_decode_n_pdu(struct iso_tp *self, struct iso_tp_can_frame *f)
{
	/* Commonly used */
	struct iso_tp_n_pdu *n_pdu    = &self->_n_pdu;
	struct iso_tp_n_pci *n_pci    = &self->_n_pdu.n_pci;
	uint8_t		     can_dl   = f->len;
	uint8_t		    *can_data = f->data;

	n_pci->n_pcitype = ISO_TP_N_PCITYPE_INVALID;

	/* ISO_TP_N_PCITYPE_SF */
	if ((can_dl >= 1u) && ((can_data[0] & 0xF0u) == 0x00u)) {
		_iso_tp_deduce_n_pcitype_n_sf(self, f);

	/* ISO_TP_N_PCITYPE_FF */
	} else if ((can_dl >= 2u) && ((can_data[0] & 0xF0u) == 0x10u)) {
		_iso_tp_deduce_n_pcitype_n_ff(self, f);

	/* ISO_TP_N_PCITYPE_CF */
	} else if ((can_dl >= 1u) && ((can_data[0] & 0xF0u) == 0x20u)) {
		/* It's a consecutive frame! */
		n_pci->n_pcitype = ISO_TP_N_PCITYPE_CF;
		n_pdu->n_pci.sn  = (can_data[0] & 0x0Fu);

		/* Copy data to N_PDU */
		(void)memcpy(n_pdu->n_data, &can_data[1], can_dl - 1u);

	/* ISO_TP_N_PCITYPE_FC */
	} else if ((can_dl >= 3u) && ((can_data[0] & 0xF0u) == 0x30u)) {
		/* Simplest case, we don't assume a shit */
		n_pci->n_pcitype = ISO_TP_N_PCITYPE_FC;
		n_pci->fs         = (can_data[0] & 0x0Fu);
		n_pci->bs         = can_data[1];
		n_pci->min_st     = can_data[2];
	} else {}
}

/** Encode N_PDU and put its content into CAN frame.
 * Caller must ensure n_pdu->n_data contains the payload for
 * THIS specific frame. Currently only Normal addressing is used TODO */
void _iso_tp_encode_n_pdu(struct iso_tp *self, struct iso_tp_can_frame *f)
{
	struct iso_tp_n_pdu *n_pdu    = &self->_n_pdu;
	struct iso_tp_n_pci *n_pci    = &self->_n_pdu.n_pci;
	uint8_t             *can_data = f->data;
	uint8_t		    *can_dl   = &f->len;

	/* Cleanup frame */
	(void)memset(can_data, 0u, 8u);

	switch (n_pci->n_pcitype) {
	case ISO_TP_N_PCITYPE_SF:
		/* SF PCI: 0000 LLLL */
		if (n_pci->sf_dl <= 7u) {
			can_data[0] = (uint8_t)(0x00u | n_pci->sf_dl);

			(void)memcpy(&can_data[1], n_pdu->n_data,
				     n_pci->sf_dl);

			*can_dl = 1u + n_pci->sf_dl;
		}
		break;

	case ISO_TP_N_PCITYPE_FF:
		/* FF PCI: 0001 LLLL LLLL LLLL */
		/* Byte 0: 0x10 | Upper 4 bits of Length */
		can_data[0] = (uint8_t)(0x10u | ((n_pci->ff_dl >> 8u) & 0x0Fu));

		/* Byte 1: Lower 8 bits of Length */
		can_data[1] = (uint8_t)(n_pci->ff_dl & 0xFFu);

		/* Payload for FF starts at index 2.
		   Standard CAN FF always has 6 bytes of payload (if full).
		  (Assuming we are sending a full frame here) */
		(void)memcpy(&can_data[2], n_pdu->n_data, 6u);

		*can_dl = 8u; /* FF Always full frame from CAN 2.0 */
		break;

	case ISO_TP_N_PCITYPE_CF: {
		uint8_t cf_payload_len = n_pci->sf_dl;

		/* CF PCI: 0010 SSSS */
		can_data[0] = (uint8_t)(0x20u | (n_pci->sn & 0x0Fu));

		/* Safety cap */
		if (cf_payload_len > 7u) {
			cf_payload_len = 7u;
		}

		(void)memcpy(&can_data[1], n_pdu->n_data, cf_payload_len);

		*can_dl = 1u + cf_payload_len;

		break;
	}

	case ISO_TP_N_PCITYPE_FC:
		/* FC PCI: 0011 FFFF */
		can_data[0] = (uint8_t)(0x30u | (n_pci->fs & 0x0Fu));
		can_data[1] = n_pci->bs;
		can_data[2] = n_pci->min_st;

		*can_dl = 3u; /* Minimum len FC */

		/* Padding (optional): 8 bytes 0x00 or 0xAA */
		/* f->len = 8u; */
		break;

	default:
		/* Should not happen if logic is correct */
		*can_dl = 0;
		break;
	}
}

/** Main instance state machine. Works step by step. Returns events during
 *  operation. Must be run inside main loop. */
enum iso_tp_event iso_tp_step(struct iso_tp *self, uint32_t delta_time_ms)
{
	/* Commonly used */
	struct iso_tp_n_pci *n_pci = &self->_n_pdu.n_pci;

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
		if (!self->_has_rx) {
			break;
		}

		/* Only listen for src messages YET */
		if (self->_can_rx_frame.id != self->_cfg.src) {
			break;
		}

		/* Validate frame src address */
		if (self->_can_rx_frame.id != self->_cfg.src) {
			break;
		}

		_iso_tp_decode_n_pdu(self, &self->_can_rx_frame);

		if (n_pci->n_pcitype == (uint8_t)ISO_TP_N_PCITYPE_INVALID) {
			/* Ignore frame */
			break;
		}

		ev = ISO_TP_EVENT_N_PDU;

		break;
	}

	default:
		break;

	}

	return ev;
}
