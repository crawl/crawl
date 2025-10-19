require.config({
    baseUrl: '/static/scripts',
    shim: {
        'client': ['jquery']
    },
    paths: {
        'jquery': '/static/scripts/contrib/jquery'
    },
    waitSeconds: 0
});

require(['client']);
