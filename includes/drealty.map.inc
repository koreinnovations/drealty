<?php

define('OFFSET', 268435456);
define('RADIUS', 85445659.4471); /* $offset / pi() */

function drealty_listing_map() {
  $listings = entity_load('drealty_listing');
  drupal_add_js('http://maps.googleapis.com/maps/api/js?sensor=false', array('type' => 'external'));
  $module_path = drupal_get_path('module', 'drealty');
  drupal_add_js($module_path . '/js/drealty.search.map.js', array('type' => 'file'));
  return theme('drealty_search_map', $listings);
}

function drealty_listing_map_get_listings() {

  $bb = $_REQUEST['bounding_box'];

  $lat_condition = $bb['south_west']['lat'] . ' AND ' . $bb['north_east']['lat'];
  $lon_condition = $bb['south_west']['lng'] . ' AND ' . $bb['north_east']['lng'];

  $markers = db_select('drealty_listing', 'l')
    ->fields('l', array('latitude', 'longitude', 'id', 'listing_id'))
    ->where("(latitude BETWEEN $lat_condition) AND (longitude BETWEEN $lon_condition)")
    ->execute()
    ->fetchAllAssoc('listing_id');


  $clustered = array();

  while (count($markers)) {
    $marker = array_pop($markers);
    $cluster = array();
    foreach ($markers as $key => $target) {
      $pixels = pixelDistance($marker->latitude, $marker->longitude, $target->latitude, $target->longitude, $bb['zoom']);
      if ($bb['distance'] > $pixels) {
        unset($markers[$key]);
        $cluster[] = $target;
      }
    }

    if (count($cluster) > 0) {
      $cluster[] = $marker;
      $data['count'] = count($cluster) + 1;
      $data['center']['lat'] = $marker->latitude;
      $data['center']['lng'] = $marker->longitude;
      $data['markers'] = $cluster;
      $clustered[] = $data;
    } else {
      $cluster[] = $marker;
      $data['count'] = 1;
      $data['center']['lat'] = $marker->latitude;
      $data['center']['lng'] = $marker->longitude;
      $data['markers'] = $cluster;
      $clustered[] = $data;
    }
  }

  print drupal_json_encode($clustered);
  exit();
}

function drealty_lonToX($lon) {
  return round(OFFSET + RADIUS * $lon * pi() / 180);
}

function drealty_latToY($lat) {
  return round(OFFSET - RADIUS *
      log((1 + sin($lat * pi() / 180)) /
        (1 - sin($lat * pi() / 180))) / 2);
}

function pixelDistance($lat1, $lon1, $lat2, $lon2, $zoom) {
  $x1 = drealty_lonToX($lon1);
  $y1 = drealty_latToY($lat1);

  $x2 = drealty_lonToX($lon2);
  $y2 = drealty_latToY($lat2);

  return sqrt(pow(($x1 - $x2), 2) + pow(($y1 - $y2), 2)) >> (21 - $zoom);
}
