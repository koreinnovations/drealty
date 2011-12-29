<?php

// $Id$
/**
 *
 * @file realpro.drush.inc
 *
 */
function drealty_drush_command() {
  $items = array();

  $items['rets-import'] = array(
      'description' => 'Import RETS Listings.',
  );

  $items['rets-flush'] = array(
      'description' => 'Flush ALL RETS Listings.',
  );

  $items['rets-list'] = array(
      'description' => 'List all available connections.',
  );

  $items['rets-status'] = array(
      'description' => 'Prints status information about dRealty imports.',
  );

  $items['rets-reprocess-images'] = array(
      'description' => "Redownload Property Images"
  );
  return $items;
}

/**
 * Implementation of hook_drush_help().
 *
 * This function is called whenever a drush user calls
 * 'drush help <name-of-command>'
 *
 * @param string $section
 * @return string
 */
function drealty_drush_help($section) {
  switch ($section) {
    case 'drush:rets-import':
      return dt('');
    case 'drush:rets-flush':
      return dt('');
    case 'drush:rets-list':
      return dt('');
    case 'drush:rets-status':
      return dt('');
    case 'drush:rets-reprocess-images':
      return dt('');
  }
}

function drush_drealty_rets_import() {
  // set the time out to infinite
  set_time_limit(0);

  module_load_include('php', 'drealty', 'drealtyDaemon');
  $daemon = new drealtyDaemon();
  $daemon->run();
}

function drush_drealty_rets_reprocess_images() {
  set_time_limit(0);

//  module_load_include('php', 'drealty', 'drealtyDaemon');
//  $daemon = new drealtyDaemon();
//  $daemon->process_images(1, 'Property');

  $listings = entity_load('drealty_listing');

  foreach ($listings as $listing) {
    $result = db_select('file_managed', 'f')
      ->fields('f', array('fid'))
      ->condition('filename', $listing->listing_id . '%', "LIKE")
      ->execute()
      ->fetchAllAssoc('fid');

    $listing->process_images = FALSE;
    $listing->save();

    $fids = array_keys($result);
    $files = file_load_multiple($fids);

    foreach ($files as $file) {
      file_usage_add($file, 'drealty', 'drealty_listing', $listing->id);
      drush_log(dt("Adding usage for @filename to listing: @id", array("@filename" => $file->filename, "@id" => $listing->id)), 'success');
    }
    unset($fids, $result, $files, $file);
  }
}

function drush_drealty_rets_flush() {
  set_time_limit(0);

  $entity_types = array('drealty_listing', 'drealty_office', 'drealty_agent', 'drealty_openhouse');

  foreach ($entity_types as $entity_type) {

    $existing_items = db_select($entity_type, "t")
      ->fields("t", array("id"))
      ->execute()
      ->fetchAll();

    foreach ($existing_items as $item) {
      $entity = reset(entity_load($entity_type, array($item->id), array(), TRUE));
      drush_log(dt('Deleting @type - @id', array('@id' => $item->id, '@type' => $entity_type)), 'success');
      $entity->delete();
      unset($entity);
    }
    unset($existing_items);
  }
}