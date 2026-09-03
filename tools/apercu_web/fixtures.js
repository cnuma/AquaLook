// Banc d'apercu : sert des donnees fictives a la place du serveur, pour
// verifier la mise en page sans session ni reseau. Rien de ceci n'est
// televerse ; le fichier d'origine n'est pas modifie.
(function(){
  const maintenant = new Date().toISOString().replace('T',' ').slice(0,23);
  const creneau = (h,m,d,on) => [h,m,d,on?1:0];
  const vide = () => [creneau(6,0,5,false),creneau(6,0,5,false),creneau(6,0,5,false),
                      creneau(6,0,5,false),creneau(6,0,5,false)];
  function jours(def){
    const out = [];
    for (let d=0; d<7; d++) out.push(def[d] ? def[d]() : vide());
    return out;
  }
  const zones = [
    { i:0, name:'Potager distant', mode:0, intervalDays:2,
      rain:{thresholdMm:2,forecastHours:24},
      days: jours({0:()=>[creneau(6,30,12,true),creneau(6,0,5,false),creneau(6,0,5,false),creneau(6,0,5,false),creneau(6,0,5,false)],
                   1:()=>[creneau(6,0,5,true),creneau(6,0,5,false),creneau(6,0,5,false),creneau(6,0,5,false),creneau(6,0,5,false)],
                   2:()=>[creneau(19,45,8,true),creneau(22,0,5,true),creneau(6,0,5,false),creneau(6,0,5,false),creneau(6,0,5,false)],
                   4:()=>[creneau(7,0,10,true),creneau(6,0,5,false),creneau(6,0,5,false),creneau(6,0,5,false),creneau(6,0,5,false)]}),
      interval: vide() },
    { i:1, name:'Z2 - Jardin', mode:1, intervalDays:3,
      rain:{thresholdMm:3,forecastHours:24}, days: jours({}),
      interval:[creneau(6,35,5,true),creneau(6,0,5,false),creneau(6,0,5,false),creneau(6,0,5,false),creneau(6,0,5,false)] },
    { i:2, name:'Zone 3', mode:0, intervalDays:2,
      rain:{thresholdMm:2,forecastHours:24},
      days: jours({2:()=>[creneau(15,0,5,true),creneau(6,0,5,false),creneau(6,0,5,false),creneau(6,0,5,false),creneau(6,0,5,false)],
                   5:()=>[creneau(8,15,20,true),creneau(12,30,7,true),creneau(6,0,5,false),creneau(6,0,5,false),creneau(6,0,5,false)]}),
      interval: vide() },
    { i:3, name:'Zone 4-4', mode:0, intervalDays:2,
      rain:{thresholdMm:2,forecastHours:24},
      days: jours({1:()=>[creneau(6,0,5,true),creneau(6,0,5,false),creneau(6,0,5,false),creneau(6,0,5,false),creneau(6,0,5,false)]}),
      interval: vide() }
  ];

  const commandes = [
    { correlationId:'c1', state:'pending', issuedAt:maintenant, source:'espace:essai',
      command:{ type:'config.apply', baseRevision:39, zones:[
        { i:2, daySlots:[{day:4, slot:0, on:true, h:7, m:15, dur:9}] } ] } },
    { correlationId:'c0', state:'refused', issuedAt:maintenant, detail:'config-modifiee-localement (attendu=37 courant=39)',
      command:{ type:'config.apply', baseRevision:37, zones:[
        { i:0, daySlots:[{day:0, slot:1, on:true, h:20, m:0, dur:6}] } ] } }
  ];

  const REPONSES = {
    '/app/me': { email:'essai@exemple.fr', label:'cnuma' },
    '/app/modules': [ { module_id:'JARDIN-01', label:'JARDIN-01', firmware:'5.9.7',
                        last_seen:maintenant, revision:39 } ],
    '/app/module': { config:{ revision:39, updatedAt:maintenant,
                              payload:{ schema:2, revision:39,
                                        system:{ nbZones:4, maxWateringMin:60 },
                                        zones:zones } },
                     derniers:[], commandes:commandes },
    '/app/backup': { id:9, revision:31, capturedAt:maintenant,
      label:'avant refonte du massif', pinned:true,
      payload:{ zones:zones, settings:{
        ntp:{server:'pool.ntp.org',gmtOffset:3600,dstOffset:3600},
        owm:{lat:48.85,lon:2.35,units:'metric',city:'Vernon',country:'FR',apiKeySet:true},
        wind:{gustKmh:30,severeKmh:50},
        touch:{xMin:300,xMax:3758,yMin:324,yMax:3790},
        display:{cBg:'#101818',cText:'#f8fcf8',rSm:4,rMd:6,rLg:10,planGap:6},
        manualDurationMin:10 } } },
    '/app/backups': [
      { id:12, revision:39, capturedAt:maintenant, label:null, pinned:false, zones:4 },
      { id:9,  revision:31, capturedAt:maintenant, label:'avant refonte du massif', pinned:true, zones:4 },
      { id:4,  revision:22, capturedAt:maintenant, label:null, pinned:false, zones:3 }
    ]
  };

  window.fetch = function(url){
    const chemin = String(url).split('?')[0];
    const corps = REPONSES[chemin];
    return Promise.resolve({ ok: corps !== undefined, status: corps !== undefined ? 200 : 404,
      text: () => Promise.resolve(JSON.stringify(corps === undefined ? {detail:'route inconnue'} : corps)) });
  };

  window.APERCU = { ouvrirJour: () => ouvrirJour(0, 2), ouvrirZone: () => ouvrirZone(0) };
  demarrer().then(() => ouvrir('JARDIN-01')).then(() => {
    document.body.dataset.pret = '1';
  });
})();
