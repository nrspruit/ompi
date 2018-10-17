/*
 * Copyright (c) 2013-2018 Intel, Inc. All rights reserved
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef MTL_OFI_OPT_H_HAS_BEEN_INCLUDED
#define MTL_OFI_OPT_H_HAS_BEEN_INCLUDED

#include "mtl_ofi.h"

BEGIN_C_DECLS

#define CQ_DATA_TYPES 2
#define __OMPI_MTL_OFI_SEND_TYPES__     [CQ_DATA_TYPES]
#define __OMPI_MTL_OFI_ISEND_TYPES__    [CQ_DATA_TYPES]
#define __OMPI_MTL_OFI_IRECV_TYPES__    [CQ_DATA_TYPES]
#define __OMPI_MTL_OFI_IPROBE_TYPES__   [CQ_DATA_TYPES]
#define __OMPI_MTL_OFI_IMPROBE_TYPES__  [CQ_DATA_TYPES]

struct ompi_mtl_ofi_send_symtable {
    int (*ompi_mtl_ofi_send __OMPI_MTL_OFI_SEND_TYPES__ )
            (struct mca_mtl_base_module_t *mtl,
              struct ompi_communicator_t *comm,
              int dest,
              int tag,
              struct opal_convertor_t *convertor,
              mca_pml_base_send_mode_t mode);
};

struct ompi_mtl_ofi_isend_symtable {
    int (*ompi_mtl_ofi_isend __OMPI_MTL_OFI_ISEND_TYPES__ )
            (struct mca_mtl_base_module_t *mtl,
               struct ompi_communicator_t *comm,
               int dest,
               int tag,
               struct opal_convertor_t *convertor,
               mca_pml_base_send_mode_t mode,
               bool blocking,
               mca_mtl_request_t *mtl_request);
};

struct ompi_mtl_ofi_irecv_symtable {
    int (*ompi_mtl_ofi_irecv __OMPI_MTL_OFI_IRECV_TYPES__ )
            (struct mca_mtl_base_module_t *mtl,
               struct ompi_communicator_t *comm,
               int src,
               int tag,
               struct opal_convertor_t *convertor,
               mca_mtl_request_t *mtl_request);
};

struct ompi_mtl_ofi_iprobe_symtable {
    int (*ompi_mtl_ofi_iprobe __OMPI_MTL_OFI_IPROBE_TYPES__ )
            (struct mca_mtl_base_module_t *mtl,
                struct ompi_communicator_t *comm,
                int src,
                int tag,
                int *flag,
                struct ompi_status_public_t *status);
};

struct ompi_mtl_ofi_improbe_symtable {
    int (*ompi_mtl_ofi_improbe __OMPI_MTL_OFI_IMPROBE_TYPES__ )
            (struct mca_mtl_base_module_t *mtl,
                 struct ompi_communicator_t *comm,
                 int src,
                 int tag,
                 int *matched,
                 struct ompi_message_t **message,
                 struct ompi_status_public_t *status);
};

struct ompi_mtl_ofi_symtables {
    struct ompi_mtl_ofi_send_symtable send_sym;
    struct ompi_mtl_ofi_isend_symtable isend_sym;
    struct ompi_mtl_ofi_irecv_symtable irecv_sym;
    struct ompi_mtl_ofi_iprobe_symtable iprobe_sym;
    struct ompi_mtl_ofi_improbe_symtable improbe_sym;
};

/**
 * MTL OFI specialization function symbol table init
 */
void ompi_mtl_ofi_send_symtable_init(struct ompi_mtl_ofi_send_symtable* sym_table);
void ompi_mtl_ofi_isend_symtable_init(struct ompi_mtl_ofi_isend_symtable* sym_table);
void ompi_mtl_ofi_irecv_symtable_init(struct ompi_mtl_ofi_irecv_symtable* sym_table);
void ompi_mtl_ofi_iprobe_symtable_init(struct ompi_mtl_ofi_iprobe_symtable* sym_table);
void ompi_mtl_ofi_improbe_symtable_init(struct ompi_mtl_ofi_improbe_symtable* sym_table);

END_C_DECLS

#endif /* MTL_OFI_OPT_H_HAS_BEEN_INCLUDED */