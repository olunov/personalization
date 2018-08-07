<?php

/**
 * @file
 * Contains SearchApiPersonalizationProcessor.
 */

/**
 * Processor for recording searching activity.
 */
class SearchApiPersonalizationProcessor extends SearchApiAbstractProcessor {

  /**
   * {@inheritdoc}
   */
  protected function process(&$value) {
    // We don't touch integers, NULL values or the like.
    if (is_string($value)) {
      module_load_include('inc', 'personalization', 'personalization.tracker');
      $search_value = drupal_strtolower($value);
      personalization_track_search($search_value);
    }
  }

}
