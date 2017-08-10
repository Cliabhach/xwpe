/**
 *  we_undo_shield.h
 *
 *  Copyright (C) 2017 Guus Bonnema
 *
 *  This is free software; You can redistribute it and/or
 *  modify it under the terms of the GNU Public License version 2 or later.
 *  See the file COPYING for license information.
 *
 */
#ifndef WE_UNDO_SHIELD_H
#define WE_UNDO_SHIELD_H

#include "config.h"

/**
 * Adds one shield to undo.
 *
 */
void shield_undo();
/**
 * removes one shield from undo.
 *
 */
void unshield_undo();
/**
 * Forcefully remove all shields from undo.
 */
void force_unshield_undo();
/**
 * Forcefully remove all shields from undo (if present) and add one shield to undo.
 */
void force_shield_undo();
/**
 * return true if undo is active and calling e_add_undo has the result of
 * adding one undo action.
 *
 */
_Bool undo_is_shielded();
/**
 * initialize undo to be unshielded.
 */
void init_undo_shield();

#endif
