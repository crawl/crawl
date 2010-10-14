/*
 *  File:       wiz-item.h
 *  Summary:    Item related wizard functions.
 *  Written by: Linley Henzell and Jesse Jones
 */

#ifndef WIZITEM_H
#define WIZITEM_H

void wizard_create_spec_object(void);
void wizard_create_spec_object_by_name();
void wizard_tweak_object(void);
void wizard_make_object_randart(void);
void wizard_value_artefact();
void wizard_uncurse_item();
void wizard_create_all_artefacts();
void wizard_identify_pack();
void wizard_unidentify_pack();
void wizard_draw_card();

void debug_item_statistics(void);

#endif
