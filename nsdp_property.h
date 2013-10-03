#ifndef NSDP_PROPERTY_H
#define NSDP_PROPERTY_H

#include "nsdp_types.h"
#include "nsdp_property_types.h"
#include "nsdp_properties.h"

struct nsdp_property_desc {
  nsdp_tag_t				tag;
  const char				*name;
  const char				*desc;

  const nsdp_property_type_t		*type;
  // repeat, etc ?
};

// Get a property desc from a tag
const struct nsdp_property_desc*
  nsdp_get_property_desc_from_tag(nsdp_tag_t tag);

// Get a property desc from a name
const struct nsdp_property_desc*
  nsdp_get_property_desc_from_name(const char *name);

// Get a property desc from a name or tag
const struct nsdp_property_desc*
  nsdp_get_property_desc(const char *name);

typedef struct nsdp_property {
  struct list_head	list;
  nsdp_tag_t		tag;
  nsdp_length_t		length;
  uint8_t		data[0];
} nsdp_property_t;

// Create a property object
nsdp_property_t*
  nsdp_property_new(unsigned tag, unsigned length);

// Create a property object with the given data
nsdp_property_t*
  nsdp_property_from_data(unsigned tag, unsigned length, const void *data);

// Create a property from its human readable form
nsdp_property_t*
  nsdp_property_from_txt(const struct nsdp_property_desc* desc,
                         const char* txt);

// Get the desc of a property
const struct nsdp_property_desc*
  nsdp_property_get_desc(const nsdp_property_t *prop);

// Get the property in human readable form
int nsdp_property_to_txt(const nsdp_property_t *prop,
                         char* txt, unsigned size);

// Free the property
void nsdp_property_free(nsdp_property_t* prop);

#endif /* NSDP_PROPERTY_H */
