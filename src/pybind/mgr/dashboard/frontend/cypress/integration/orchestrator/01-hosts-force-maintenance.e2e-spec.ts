import { HostsPageHelper } from '../cluster/hosts.po';

describe('Hosts page', () => {
  const hosts = new HostsPageHelper();

  beforeEach(() => {
    cy.login();
    Cypress.Cookies.preserveOnce('token');
    hosts.navigateTo();
  });

  describe('when Orchestrator is available', () => {
    beforeEach(function () {
      cy.fixture('orchestrator/inventory.json').as('hosts');
      cy.fixture('orchestrator/services.json').as('services');
    });

    it('should force enter host into maintenance', function () {
      const hostname = Cypress._.sample(this.hosts).name;
      this.services.forEach((service: any) => {
        if (hostname === service.hostname) {
          if (
            service.daemon_type !== 'rgw' &&
            (service.daemon_type === 'mgr' || service.daemon_type === 'alertmanager')
          ) {
            enterMaintenance = false;
          }
        }
      });

      let enterMaintenance = true;

      // Force maintenance might throw out error if host are less than 3.
      if (this.hosts.length < 3) {
        enterMaintenance = false;
      }

      if (enterMaintenance) {
        hosts.maintenance(hostname, true, true);
      }
    });
  });
});
