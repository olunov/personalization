<?php
/**
 * @file
 * Renders one node link in the big listing
 */
 ?>
 <li>
<p><?php echo l(check_plain($item->title), 'node/' . $item->nid); ?></p>
<?php echo $item->teaser; ?>
</li>
