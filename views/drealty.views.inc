<?php

function drealty_views_data_alter(&$data) {
  $blah = $data;
  $data['drealty_connections']['edit_connection'] = array(
      'field' => array(
          'title' => t('Edit Connection Link'),
          'help' => t('Provide a link to the edit form for the connection'),
          'handler' => 'drealty_connection_handler_edit_link_field',
      ),
  );
  $data['drealty_connections']['operations'] = array(
      'field' => array(
          'title' => t('Operations links'),
          'help' => t('Display all operations available for this connection.'),
          'handler' => 'drealty_connection_handler_connection_operations_field',
      ),
  );
  $data['drealty_listing']['operations'] = array(
      'field' => array(
          'title' => t('Operations Links'),
          'help' => t('Display all operations available for this listing.'),
          'handler' => 'drealty_listing_handler_operations_field',
      ),
  );
  $data['drealty_listing']['mls_images'] = array(
      'field' => array(
          'title' => t('MLS Images'),
          'help' => t('Display images pulled from the MLS.'),
          'handler' => 'drealty_listing_handler_mls_images_field',
      ),
  );
  $data['drealty_listing']['mls_address'] = array(
      'field' => array(
          'title' => t('MLS Composite Address'),
          'help' => t('Display a formatted address. Street City, State Zip.'),
          'handler' => 'drealty_listing_handler_mls_address_field',
      ),
  );
  $data['drealty_listing']['office_id'] = array(
      'title' => t('Office Id'),
      'field' => array('real field' => 'list_office_mls_id'),
      'filter' => array('handler' => 'drealty_listing_filter_office_id'),
  );
  $data['drealty_openhouse']['listing_id']['relationship'] = array(
      'handler' => 'views_handler_relationship',
      'base' => 'drealty_listing',
      'base field' => 'listing_id',
      'title' => t('Drealty Listing'),
      'label' => t('Get the Listing associated with an Open House.'),
  );

  $data['drealty_listing']['list_agent_mls_id']['relationship'] = array(
      'handler' => 'views_handler_relationship',
      'base' => 'drealty_agent',
      'base field' => 'agent_id',
      'title' => t('Drealty Listing Agent'),
      'label' => t('Drealty Listing Agent.'),
      'help' => t('List all the listings for the listing agent'),
  );

  $data['drealty_agent']['agent_id']['relationship'] = array(
      'handler' => 'views_handler_relationship',
      'base' => 'drealty_listing',
      'base field' => 'list_agent_mls_id',
      'title' => t('Drealty Listing Agent'),
      'label' => t('Drealty Listing Agent.'),
      'help' => t('List all the listings for the listing agent'),
  );
}