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

  $max_before = db_query("SELECT MAX(created) as max_created, MAX(changed) as max_changed FROM drealty_listing")->fetchObject();  
  $start_time = time();
  watchdog('message', 'RETS Import Starts at !start.', array('!start' => $start_time));
  module_load_include('php', 'drealty', 'drealtyDaemon');
  $daemon = new drealtyDaemon();
  $daemon->run();
  $max_after = db_query("SELECT MAX(created) as max_created, MAX(changed) as max_changed FROM drealty_listing")->fetchObject();  
  $end_time = time();
  watchdog('message', 'RETS Import Ends at !end.', array('!end' => $end_time));
  module_load_include('inc', 'reman', 'includes/pages');
  realtsoft_listing_import_manipulation($start_time, $end_time, $max_before, $max_after);
  
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
  $listings = entity_load("drealty_listing", FALSE, array(), TRUE);
  foreach ($listings as $listing) {
    drush_log(dt('Deleting listing @id', array('@id' => $listing->id)), 'success');
    $listing->delete();
  }
}