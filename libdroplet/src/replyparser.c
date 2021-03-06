/*
 * Copyright (C) 2010 SCALITY SA. All rights reserved.
 * http://www.scality.com
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY SCALITY SA ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SCALITY SA OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of SCALITY SA.
 *
 * https://github.com/scality/Droplet
 */
#include "dropletp.h"

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

dpl_status_t
dpl_get_metadata_from_headers(dpl_dict_t *headers,
                              dpl_dict_t *metadata)
{
  return dpl_dict_filter_prefix(metadata, headers, "x-amz-meta-");
}

/**/

void
dpl_bucket_free(dpl_bucket_t *bucket)
{
  free(bucket->name);
  free(bucket);
}

void
dpl_vec_buckets_free(dpl_vec_t *vec)
{
  int i;

  for (i = 0;i < vec->n_items;i++)
    dpl_bucket_free((dpl_bucket_t *) vec->array[i]);
  dpl_vec_free(vec);
}

static dpl_status_t
parse_list_all_my_buckets_bucket(xmlNode *node,
                                 dpl_vec_t *vec)
{
  xmlNode *tmp;
  dpl_bucket_t *bucket = NULL;
  int ret;

  bucket = malloc(sizeof (*bucket));
  if (NULL == bucket)
    goto bad;

  memset(bucket, 0, sizeof (*bucket));

  for (tmp = node; NULL != tmp; tmp = tmp->next)
    {
      if (tmp->type == XML_ELEMENT_NODE)
        {
          DPRINTF("name: %s\n", tmp->name);
          if (!strcmp((char *) tmp->name, "Name"))
            {
              bucket->name = strdup((char *) tmp->children->content);
              if (NULL == bucket->name)
                goto bad;
            }

          if (!strcmp((char *) tmp->name, "CreationDate"))
            {
              bucket->creation_time = dpl_iso8601totime((char *) tmp->children->content);
            }

        }
      else if (tmp->type == XML_TEXT_NODE)
        {
          DPRINTF("content: %s\n", tmp->content);
        }
    }

  ret = dpl_vec_add(vec, bucket);
  if (DPL_SUCCESS != ret)
    goto bad;

  return DPL_SUCCESS;

 bad:

  if (NULL != bucket)
    dpl_bucket_free(bucket);

  return DPL_FAILURE;
}

static dpl_status_t
parse_list_all_my_buckets_buckets(xmlNode *node,
                                  dpl_vec_t *vec)
{
  xmlNode *tmp;
  int ret;

  for (tmp = node; NULL != tmp; tmp = tmp->next)
    {
      if (tmp->type == XML_ELEMENT_NODE)
        {
          DPRINTF("name: %s\n", tmp->name);

          if (!strcmp((char *) tmp->name, "Bucket"))
            {
              ret = parse_list_all_my_buckets_bucket(tmp->children, vec);
              if (DPL_SUCCESS != ret)
                return DPL_FAILURE;
            }

        }
      else if (tmp->type == XML_TEXT_NODE)
        {
          DPRINTF("content: %s\n", tmp->content);
        }
    }

  return DPL_SUCCESS;
}

static dpl_status_t
parse_list_all_my_buckets_children(xmlNode *node,
                                   dpl_vec_t *vec)
{
  xmlNode *tmp;
  int ret;

  for (tmp = node; NULL != tmp; tmp = tmp->next)
    {
      if (tmp->type == XML_ELEMENT_NODE)
        {
          DPRINTF("name: %s\n", tmp->name);

          if (!strcmp((char *) tmp->name, "Buckets"))
            {
              ret = parse_list_all_my_buckets_buckets(tmp->children, vec);
              if (DPL_SUCCESS != ret)
                return DPL_FAILURE;
            }
        }
      else if (tmp->type == XML_TEXT_NODE)
        {
          DPRINTF("content: %s\n", tmp->content);
        }
    }

  return DPL_SUCCESS;
}

dpl_status_t
dpl_parse_list_all_my_buckets(dpl_ctx_t *ctx,
                              char *buf,
                              int len,
                              dpl_vec_t *vec)
{
  xmlParserCtxtPtr ctxt = NULL;
  xmlDocPtr doc = NULL;
  int ret;
  xmlNode *tmp;
  //ssize_t cc;

  //cc = write(1, buf, len);

  if ((ctxt = xmlNewParserCtxt()) == NULL)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  doc = xmlCtxtReadMemory(ctxt, buf, len, NULL, NULL, 0u);
  if (NULL == doc)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  for (tmp = xmlDocGetRootElement(doc); NULL != tmp; tmp = tmp->next)
    {
      if (tmp->type == XML_ELEMENT_NODE)
        {
          DPRINTF("name: %s\n", tmp->name);

          if (!strcmp((char *) tmp->name, "ListAllMyBucketsResult"))
            {
              ret = parse_list_all_my_buckets_children(tmp->children, vec);
              if (DPL_SUCCESS != ret)
                return DPL_FAILURE;
            }
        }
      else if (tmp->type == XML_TEXT_NODE)
        {
          DPRINTF("content: %s\n", tmp->content);
        }
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != doc)
    xmlFreeDoc(doc);

  if (NULL != ctxt)
    xmlFreeParserCtxt(ctxt);

  return ret;
}

/**/

void
dpl_object_free(dpl_object_t *object)
{
  if (NULL != object->key)
    free(object->key);

  free(object);
}

void
dpl_vec_objects_free(dpl_vec_t *vec)
{
  int i;

  for (i = 0;i < vec->n_items;i++)
    dpl_object_free((dpl_object_t *) vec->array[i]);
  dpl_vec_free(vec);
}

void
dpl_common_prefix_free(dpl_common_prefix_t *common_prefix)
{
  if (NULL != common_prefix->prefix)
    free(common_prefix->prefix);

  free(common_prefix);
}

void
dpl_vec_common_prefixes_free(dpl_vec_t *vec)
{
  int i;

  for (i = 0;i < vec->n_items;i++)
    dpl_common_prefix_free((dpl_common_prefix_t *) vec->array[i]);
  dpl_vec_free(vec);
}

static dpl_status_t
parse_list_bucket_content(xmlNode *node,
                          dpl_vec_t *vec)
{
  xmlNode *tmp;
  dpl_object_t *object = NULL;
  int ret;

  object = malloc(sizeof (*object));
  if (NULL == object)
    goto bad;

  memset(object, 0, sizeof (*object));

  for (tmp = node; NULL != tmp; tmp = tmp->next)
    {
      if (tmp->type == XML_ELEMENT_NODE)
        {
          DPRINTF("name: %s\n", tmp->name);
          if (!strcmp((char *) tmp->name, "Key"))
            {
              object->key = strdup((char *) tmp->children->content);
              if (NULL == object->key)
                goto bad;
            }
          else if (!strcmp((char *) tmp->name, "LastModified"))
            {
              object->last_modified = dpl_iso8601totime((char *) tmp->children->content);
            }
          else if (!strcmp((char *) tmp->name, "Size"))
            {
              object->size = strtoull((char *) tmp->children->content, NULL, 0);
            }

        }
      else if (tmp->type == XML_TEXT_NODE)
        {
          DPRINTF("content: %s\n", tmp->content);
        }
    }

  ret = dpl_vec_add(vec, object);
  if (DPL_SUCCESS != ret)
    goto bad;

  return DPL_SUCCESS;

 bad:

  if (NULL != object)
    dpl_object_free(object);

  return DPL_FAILURE;
}

static dpl_status_t
parse_list_bucket_common_prefixes(xmlNode *node,
                                  dpl_vec_t *vec)
{
  xmlNode *tmp;
  dpl_common_prefix_t *common_prefix = NULL;
  int ret;

  common_prefix = malloc(sizeof (*common_prefix));
  if (NULL == common_prefix)
    goto bad;

  memset(common_prefix, 0, sizeof (*common_prefix));

  for (tmp = node; NULL != tmp; tmp = tmp->next)
    {
      if (tmp->type == XML_ELEMENT_NODE)
        {
          DPRINTF("name: %s\n", tmp->name);
          if (!strcmp((char *) tmp->name, "Prefix"))
            {
              common_prefix->prefix = strdup((char *) tmp->children->content);
              if (NULL == common_prefix->prefix)
                goto bad;
            }
        }
      else if (tmp->type == XML_TEXT_NODE)
        {
          DPRINTF("content: %s\n", tmp->content);
        }
    }

  ret = dpl_vec_add(vec, common_prefix);
  if (DPL_SUCCESS != ret)
    goto bad;

  return DPL_SUCCESS;

 bad:

  if (NULL != common_prefix)
    dpl_common_prefix_free(common_prefix);

  return DPL_FAILURE;
}

static dpl_status_t
parse_list_bucket_children(xmlNode *node,
                           dpl_vec_t *objects,
                           dpl_vec_t *common_prefixes)
{
  xmlNode *tmp;
  int ret;

  for (tmp = node; NULL != tmp; tmp = tmp->next)
    {
      if (tmp->type == XML_ELEMENT_NODE)
        {
          DPRINTF("name: %s\n", tmp->name);

          if (!strcmp((char *) tmp->name, "Contents"))
            {
              ret = parse_list_bucket_content(tmp->children, objects);
              if (DPL_SUCCESS != ret)
                return DPL_FAILURE;
            }
          else if (!strcmp((char *) tmp->name, "CommonPrefixes"))
            {
              ret = parse_list_bucket_common_prefixes(tmp->children, common_prefixes);
              if (DPL_SUCCESS != ret)
                return DPL_FAILURE;
            }
        }
      else if (tmp->type == XML_TEXT_NODE)
        {
          DPRINTF("content: %s\n", tmp->content);
        }
    }

  return DPL_SUCCESS;
}

dpl_status_t
dpl_parse_list_bucket(dpl_ctx_t *ctx,
                      char *buf,
                      int len,
                      dpl_vec_t *objects,
                      dpl_vec_t *common_prefixes)
{
  xmlParserCtxtPtr ctxt = NULL;
  xmlDocPtr doc = NULL;
  int ret;
  xmlNode *tmp;
  //ssize_t cc;

  //cc = write(1, buf, len);

  if ((ctxt = xmlNewParserCtxt()) == NULL)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  doc = xmlCtxtReadMemory(ctxt, buf, len, NULL, NULL, 0u);
  if (NULL == doc)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  for (tmp = xmlDocGetRootElement(doc); NULL != tmp; tmp = tmp->next)
    {
      if (tmp->type == XML_ELEMENT_NODE)
        {
          DPRINTF("name: %s\n", tmp->name);

          if (!strcmp((char *) tmp->name, "ListBucketResult"))
            {
              ret = parse_list_bucket_children(tmp->children, objects, common_prefixes);
              if (DPL_SUCCESS != ret)
                return DPL_FAILURE;
            }
        }
      else if (tmp->type == XML_TEXT_NODE)
        {
          DPRINTF("content: %s\n", tmp->content);
        }
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != doc)
    xmlFreeDoc(doc);

  if (NULL != ctxt)
    xmlFreeParserCtxt(ctxt);

  return ret;
}

/**/


