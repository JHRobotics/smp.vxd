// https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.9.0.pdf

// p.43: 7.5 Processor Information (Type 4)

typedef struct smbios_2_entry
{
	uint8_t anchor[4]; // _SM_
	uint8_t checksum;
	uint8_t length;
	uint8_t major;
	uint8_t minor;
	uint16_t max_size;
	uint8_t revision;
	uint8_t formated_area[5];
	uint8_t im_anchor[5]; // _DMI_
	uint16_t table_length;
	uint32_t table_address;
	uint16_t table_numbers;
	uint8_t bcd_revision;
} smbios_2_entry_t;

typedef struct smbios_3_entry
{
	uint8_t anchor[5]; // _SM3_
	uint8_t checksum;
	uint8_t length;
	uint8_t major;
	uint8_t minor;
	uint8_t docrev;
	uint8_t revision;
	uint8_t reserved;
	uint32_t max_size;
	uint32_t table_address_low;
	uint32_t table_address_high;
} smbios_3_entry_t;


