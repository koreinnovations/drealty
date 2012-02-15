<?php

/**
 * @file
 * Hooks provided by the Facet API module.
 */

/**
 * @addtogroup hooks
 * @{
 */



/**
 * Called after the rets_import is complete. 
 */
function hook_drealty_rets_import_complete() {
  
}

/**
 * Called before a Entity is saved during a RETS Import.
 * @param DrealtyListing $entity
 * @return Entity 
 */
function hook_drealty_import_presave(Entity $entity) {
  if($entity->type == 'drealty_listing') {
    $entity->price = '0';
  }
  
  return $entity;
}

/**
 * Called after a Entity is saved during a RETS Import
 * @param DrealtyListing $entity
 * @return Entity 
 */
function hook_drealty_import_save(Entity $entity) {  
  return $entity;
}



/**
 * @} End of "addtogroup hooks".
 */
