import type {SidebarsConfig} from '@docusaurus/plugin-content-docs';

/**
 * Creating a sidebar enables you to:
 * - create an ordered group of docs
 * - render a sidebar for each doc of that group
 * - provide next/previous navigation
 *
 * The sidebars can be generated from the filesystem, or explicitly defined here.
 *
 * For more details, see:
 * https://docusaurus.io/docs/sidebar
 */
const sidebars: SidebarsConfig = {
  tutorialSidebar: [
    {
      type: 'doc',
      id: 'intro',
      label: 'Introduction',
    },
    {
      type: 'category',
      label: 'Getting Started',
      items: [
        'getting-started/requirements',
        'getting-started/installation',
        'getting-started/quick-start',
        'getting-started/build-from-source',
      ],
    },
    {
      type: 'category',
      label: 'Features',
      items: [
        'features/overview',
        'features/what-is-left',
      ],
    },
    {
      type: 'category',
      label: 'Development',
      items: [
        'development/architecture',
        'development/tech-stack',
      ],
    },
    {
      type: 'doc',
      id: 'roadmap',
      label: 'Roadmap',
    },
    {
      type: 'doc',
      id: 'faq',
      label: 'FAQ',
    },
  ],
};

export default sidebars;
