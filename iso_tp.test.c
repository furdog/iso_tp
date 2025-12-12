#include "iso_tp.h"

#include <assert.h>
#include <stdio.h>

#include "example_log.h"

/******************************************************************************
 * HELPERS
 *****************************************************************************/
void iso_tp_print_n_pdu(struct iso_tp *self)
{
	struct iso_tp_n_pdu *n_pdu    = &self->_n_pdu;
	struct iso_tp_n_pci *n_pci    = &self->_n_pdu.n_pci;

	uint8_t i;

	const char *n_pcitype_str[] = {
		"ISO_TP_N_PCITYPE_INVALID",
		"ISO_TP_N_PCITYPE_SF",
		"ISO_TP_N_PCITYPE_FF",
		"ISO_TP_N_PCITYPE_CF",
		"ISO_TP_N_PCITYPE_FC",
	};

	printf("-- N_PDU BEGIN --\n");
	printf("\tN_PCI_Type: %s\n", n_pcitype_str[n_pci->n_pcitype]);

	switch (n_pci->n_pcitype) {
	case ISO_TP_N_PCITYPE_SF:
		printf("\t\tSF_DL: %u\n", n_pci->sf_dl);
		break;

	case ISO_TP_N_PCITYPE_FF:
		printf("\t\tmin(FF_DL): %u\n", self->_cfg.min_ff_dl);
		printf("\t\tFF_DL     : %u\n", n_pci->ff_dl);
		break;

	case ISO_TP_N_PCITYPE_CF:
		printf("\tSN: %u\n", n_pci->sn);
		break;

	case ISO_TP_N_PCITYPE_FC:
		printf("\tFS     : %u\n", n_pci->fs);
		printf("\tBS     : %u\n", n_pci->bs);
		printf("\tmin(ST): %ums\n", n_pci->min_st);
		break;
	default:
		break;
	}

	printf("\tN_Data (len=%i)\n\t\t", n_pdu->len_n_data);
	for (i = 0; i < n_pdu->len_n_data; i++) {
		printf("0x%02X ", n_pdu->n_data[i]);
	}
	printf("\n");

	printf("-- N_PDU END   --\n\n\n\n");
}

/******************************************************************************
 * TESTS
 *****************************************************************************/
void iso_tp_test_init(struct iso_tp *self)
{
	struct iso_tp_config cfg;

	iso_tp_init(self);

	/* Usage without configuration must fail */
	assert(iso_tp_step(self, 0u) == ISO_TP_EVENT_INVALID_CONFIG);

	/* Get current configuration */
	iso_tp_get_config(self, &cfg);

	/* Set new configuration */
	cfg.tx_dl = 8u; /* CAN2.0 */
	iso_tp_set_config(self, &cfg);

	/* After configuration usage with no events - is OK */
	assert(iso_tp_step(self, 0u) == ISO_TP_EVENT_NONE);
	assert(iso_tp_step(self, 0u) == ISO_TP_EVENT_NONE);
}

/* Test single frame simplest case */
void iso_tp_test_simple_sf(struct iso_tp *self)
{
	size_t i;

	for (i = 0; i < sizeof(example_log) / sizeof(struct example_can_frame);
	     i++) {
		struct iso_tp_can_frame f;

		/* Copy example frame from log*/
		f.id   = example_log[i].id;
		f.len  = example_log[i].dlc;
		memcpy(&f.data, example_log[i].data, f.len);

		/* Push frame into iso_tp state machine */
		iso_tp_push_frame(self, &f);
		assert(iso_tp_step(self, 0u) == ISO_TP_EVENT_N_PDU);
		iso_tp_print_n_pdu(self); /* TODO no private access */
	}
}

int main ()
{
	struct iso_tp tp;

	iso_tp_test_init(&tp);
	iso_tp_test_simple_sf(&tp);

	return 0;
}
