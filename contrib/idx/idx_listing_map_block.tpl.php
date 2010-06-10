<?php

$map_array = array(
        'id' => 'myMap',
        'maptype' => "Normal",
        'controltype' => "None",
        'zoom' => 15,
        'width' => "250px",
        'height' => "250px",
        'latitude' => $node->drealty_property_loc[0]['latitude'],
        'longitude' => $node->drealty_property_loc[0]['longitude'],
        'behavior' => array(
                'locpick' => FALSE,
                'nodrag' => FALSE,
                'nokeyboard' => TRUE,
                'overview' => FALSE,
                'scale' => TRUE,
                'autozoom' => FALSE,
                'scale' => FALSE,
        ),

        'markers' => array(
                array(
                    'latitude' => $node->drealty_property_loc[0]['latitude'],
                    'longitude' => $node->drealty_property_loc[0]['longitude'],
                ),

        ),

);

print theme('gmap', array('#settings' => $map_array));

?>
