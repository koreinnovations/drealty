<?php
  $images = $mls_images;
  $item = $listing;
?>

<strong>some images</strong>
<?php

foreach($mls_images as $image) {  
  print theme('image', array('path' => $image->uri, 'alt' => $listing->name));
}

?>