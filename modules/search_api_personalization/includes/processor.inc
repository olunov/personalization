<?php

/**
 * @file
 * Contains SearchApiPersonalizationProcessor.
 */

class SearchApiPersonalizationProcessor extends SearchApiAbstractProcessor {

  /**
   * {@inheritdoc}
   */
  public function postprocessSearchResults(array &$response, SearchApiQuery $query) {
    dsm($query->getOriginalKeys());
    dsm($response);
  }
}