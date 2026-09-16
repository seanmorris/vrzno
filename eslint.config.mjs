import smNoSaccadeStyle from 'sm-no-saccade-style';
import {parse} from 'espree';

export default [
	...smNoSaccadeStyle.configs.recommended
	, {
		files: ['vrzno_*.js', 'php_stream_fetch_real_open.js']
		, languageOptions: {
			// These files are function bodies embedded by EM_JS/EM_ASYNC_JS.
			sourceType: 'module'
			, parser: {
				parse(source, options) {
					return parse(source, {
						...options
						, ecmaFeatures: {...options.ecmaFeatures, globalReturn: true}
					});
				}
			}
		}
	}
];
