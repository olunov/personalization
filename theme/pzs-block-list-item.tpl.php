<?php
/**
 * @file
 * Renders one node link in the block
 */
 ?>
 <li>
<?php echo l(check_plain($item->title), 'node/' . $item->nid); ?>
</li>
