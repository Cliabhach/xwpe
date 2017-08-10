/**
 *  we_undo_shield.c
 *
 *  Copyright (C) 2017 Guus Bonnema
 *
 *  This is free software; You can redistribute it and/or
 *  modify it under the terms of the GNU Public License version 2 or later.
 *  See the file COPYING for license information.
 *
 */

#include "config.h"
#include "we_undo_shield.h"

/**
 * e_undo_sw is a disabler for e_add_undo. when zero it enables undo, when non zero
 * it disables e_add_undo.
 *
 * Use shield_undo(), unshield_undo() or force_shield_undo() and force_unshield_undo()
 * to add or remove shields.
 *
 * This definition is declared static and only accessible from this file in order to
 * hide it's existance from other programs. Other modules should use the functions named
 * above.
 *
 */
static unsigned int e_undo_sw;

/**
 * Adds one shield to undo.
 *
 */
void shield_undo()
{
    e_undo_sw++;
}
/**
 * removes one shield from undo.
 *
 */
void unshield_undo()
{
    if (e_undo_sw > 0)
        e_undo_sw--;
}
/**
 * Forcefully remove all shields from undo.
 */
void force_unshield_undo()
{
    e_undo_sw = 0;
}
/**
 * Forcefully remove all shields from undo (if present) and add one shield to undo.
 */
void force_shield_undo()
{
    force_unshield_undo();
    shield_undo();
}
/**
 * return true if undo is active and calling e_add_undo has the result of
 * adding one undo action.
 *
 */
_Bool undo_is_shielded()
{
    return e_undo_sw == 0;
}
/**
 * initialize undo to unshielded.
 */
void init_undo_shield()
{
    force_unshield_undo();
}
