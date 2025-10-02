require.config({
    baseUrl: '/static/scripts',
    shim: {
        'client': ['jquery']
    },
    paths: {
        'jquery': '/static/scripts/contrib/jquery'
    },
    waitSeconds: 0,
    urlArgs: game_version ? 'v=' + encodeURIComponent(game_version) : undefined,
});

require(['client']);
