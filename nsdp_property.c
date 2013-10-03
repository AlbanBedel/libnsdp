#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "nsdp_property.h"

nsdp_property_t*
  nsdp_property_new(unsigned tag, unsigned length)
{
  nsdp_property_t *prop;

  prop = calloc(1, sizeof(*prop) + length);
  if (!prop)
    return NULL;

  INIT_LIST_HEAD(&prop->list);
  prop->tag = tag;
  prop->length = length;

  return prop;
}

nsdp_property_t*
  nsdp_property_from_data(unsigned tag, unsigned length, const void* data)
{
  nsdp_property_t *prop;

  prop = nsdp_property_new(tag, length);
  if (!prop)
    return NULL;

  if (length > 0)
    memcpy(prop->data, data, length);

  return prop;
}

nsdp_property_t*
  nsdp_property_from_txt(const struct nsdp_property_desc* desc,
                         const char* txt)
{
  nsdp_property_t* prop;
  int len;

  if (!desc || !txt)
    return NULL;

  len = desc->type->from_text(txt, NULL, 0);
  if (len < 0)
    return NULL;

  prop = nsdp_property_new(desc->tag, len);
  if (!prop)
    return NULL;

  len = desc->type->from_text(txt, prop->data, len);
  if (len < 0) {
    nsdp_property_free(prop);
    return NULL;
  }

  return prop;
}

const struct nsdp_property_desc*
  nsdp_property_get_desc(const nsdp_property_t *prop)
{
  return prop ? nsdp_get_property_desc_from_tag(prop->tag) : NULL;
}

int nsdp_property_to_txt(const nsdp_property_t *prop, char* txt,
                         unsigned size)
{
  const struct nsdp_property_desc* desc = nsdp_property_get_desc(prop);
  return desc ?
    desc->type->to_text(prop->data, prop->length, txt, size) :
    -EINVAL;
}

void nsdp_property_free(nsdp_property_t *prop)
{
  list_del(&prop->list);
  free(prop);
}
