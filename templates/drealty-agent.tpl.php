<h1><?php print $drealty_agent->first_name . ' '. $drealty_agent->last_name; ?></h1>

<div class="<?php print $classes; ?> clearfix"<?php print $attributes; ?>>
  <?php if (!$page): ?>
    <h2<?php print $title_attributes; ?>>
      <a href="<?php print $url; ?>"><?php print $title; ?></a>
    </h2>
  <?php endif; ?>

  <div class="content"<?php print $content_attributes; ?>>
    <?php
    print render($content);
    ?>
  </div>
</div>

