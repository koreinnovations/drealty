<?php
$street_number = isset($drealty_listing->street_number) ? $drealty_listing->street_number : '';
$street_name = isset($drealty_listing->street_name) ? $drealty_listing->street_name : '';
$street_suffix = isset($drealty_listing->street_suffix) ? $drealty_listing->street_suffix : '';

$street = "{$street_number} {$street_name} {$street_suffix}";
?>

<div class="search-result clearfix">
  <div class="listing clearfix">
    <!-- <div class="listing-image">
      <?php $image = array_shift($drealty_listing->mls_images); ?>
      <?php $img = theme_image_style(array('style_name' => 'listing_search_thumb', 'path' => $image->uri, 'alt' => '', 'width'=>'', 'height' => '')); ?>
      <?php print l($img, "listings/{$drealty_listing->listing_id}", array('html' => TRUE)); ?>
    </div> -->
    <div class="listing-data">
      <div class="listing-header clearfix">
        <div class="title-link"> <?php print l("{$street} {$drealty_listing->city}, {$drealty_listing->state_or_province} {$drealty_listing->postal_code}", "listings/{$drealty_listing->listing_id}", array('html' => TRUE)); ?></div>
        <div class="action-links">
          <?php if ($drealty_listing->virtual_tour_url): ?>
            <?php print l(t('vtour'), $drealty_listing->virtual_tour_url, array('attributes' => array('target' => '_blank', 'title' => 'view virtual tour'))); ?> |
          <?php endif; ?>
          <?php //print flag_create_link('saved_listing', $drealty_listing->id); ?>
        </div>
      </div>
      <div class="listing-data-top clearfix">
        <div class="listing-price">$<?php print number_format($drealty_listing->list_price, 0); ?></div>
        <div class="listing-school-district"><?php if ($drealty_listing->school_district): ?>School District: <?php print $drealty_listing->school_district; ?><?php else: ?>&nbsp;<?php endif; ?></div>
      </div>
      <div class="listing-feature-line">
        <?php if ($drealty_listing->beds_total): ?>
          <?php print $drealty_listing->beds_total; ?> Beds
        <?php endif; ?>
        <?php if ($drealty_listing->baths_full): ?>
          , <?php print $drealty_listing->baths_full; ?> Bath
        <?php endif; ?>
        <?php if ($drealty_listing->building_area_total): ?>
          | <?php print $drealty_listing->building_area_total; ?> Sq Ft
        <?php endif; ?>
      </div>
      <div class="listing-property-type"><?php print ucwords(drupal_strtolower($drealty_listing->property_sub_type)); ?></div>
      <div class="listing-description">
        <?php print drealty_word_limit(drealty_sentence_case($drealty_listing->public_remarks)); ?>
      </div>
    </div>
  </div>
</div>