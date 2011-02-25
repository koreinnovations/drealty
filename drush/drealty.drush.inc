<?php
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
  switch($section) {
    case 'drush:rets-import':
      return dt('');
    case 'drush:rets-flush':
      return dt('');
    case 'drush:rets-list':
      return dt('');
    case 'drush:rets-status':
      return dt('');
  }
}

function drush_drealty_rets_import() {
  // set the time out to infinite
  set_time_limit(0);

  module_load_include('php', 'drealty', 'daemon/drealty_daemon');
  $daemon = new drealty_daemon();
  $daemon->run();

}

function drush_drealty_rets_flush() {
  set_time_limit(0);

  $result = db_query("SELECT nid FROM {node} WHERE type = 'drealty_property' OR type = 'drealty_agent' OR type = 'drealty_office' OR type = 'drealty_open_house'", $conid);

  $count = 0;
  while ($nid = db_fetch_object($result)) {
    node_delete($nid->nid);
    drush_log(dt('Deleting node id: @nid. ', array('@nid' => $nid->nid)));
    $count++;
  }
  drush_log(dt('Deleted @count nodes', array('@count' => $count)), 'success');
}